// Prelude — Nintendo Switch homebrew for the Nextendo Network.
// Copyright (C) 2026 Nextendo Network
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
// PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along
// with this program. If not, see <https://www.gnu.org/licenses/>.

// ============================================================
//  Nextendo .nro — Splatoon 2 schedule installer via LayeredFS.
//
//  Extracts schedule payloads (coopdata, vsdata, fesdata) from the server's
//  NXBC bundle and writes them into Atmosphere's LayeredFS override path:
//    sdmc:/atmosphere/contents/<title_id>/romfs/DebugUnderPilot/bcat/
//  Works for both USA (01003BC0000A0000) and EUR (0100F8F0000A2000) versions.
//
//  BCAT SaveData was replaced by LayeredFS because Splatoon 2 reads its
//  schedules from ROMFS, not the BCAT delivery cache. Atmosphere's fs.mitm
//  intercepts the ROMFS reads at the fsp-srv level and serves our files instead.
// ============================================================
#include <switch.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "nextendo_bcat.h"
#include "nextendo_net.h"
#include "nextendo_config.h"

#define S2_TITLE_ID_USA "01003BC0000A0000"
#define S2_TITLE_ID_EUR "0100F8F0000A2000"
#define LAYEREDFS_BASE  "sdmc:/atmosphere/contents/%s/romfs/DebugUnderPilot/bcat"
#define BCAT_HOST       NEXTENDO_SERVER_HOST
#define BCAT_PORT       443
#define BCAT_PATH       "/api/bcat/0100f8f0000a2000/cache"
#define LOG_PATH        "sdmc:/nextendo_bcat.log"

static FILE *g_log = NULL;
Result g_last_rc = 0;
static void logf_(const char *fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

static void ensureParent(const char *filePath) {
    char dir[FS_MAX_PATH];
    size_t len = strnlen(filePath, sizeof(dir) - 1);
    memcpy(dir, filePath, len);
    dir[len] = '\0';
    char *slash = strrchr(dir, '/');
    if (!slash) return;
    *slash = '\0';
    char *p = strchr(dir, ':');
    p = p ? p + 1 : dir;
    if (*p == '/') p++;
    for (; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(dir, 0777); *p = '/'; }
    }
    mkdir(dir, 0777);
}

static void wipeTree(const char *path) {
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *e;
    char child[FS_MAX_PATH];
    while ((e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
        struct stat st;
        if (stat(child, &st) == 0 && S_ISDIR(st.st_mode)) {
            wipeTree(child);
            rmdir(child);
        } else {
            remove(child);
        }
    }
    closedir(d);
}

static void clearLayeredFS(const char *base) {
    char p[FS_MAX_PATH];
    snprintf(p, sizeof(p), "%s/coopdata", base); wipeTree(p); rmdir(p);
    snprintf(p, sizeof(p), "%s/vsdata",    base); wipeTree(p); rmdir(p);
    snprintf(p, sizeof(p), "%s/fesdata",   base); wipeTree(p); rmdir(p);
    snprintf(p, sizeof(p), "%s/dummy",     base); wipeTree(p); rmdir(p);
}

static bool writeFileB(const char *path, const unsigned char *data, u32 len) {
    ensureParent(path);
    FILE *f = fopen(path, "wb");
    if (!f) { logf_("  ECHEC fopen %s", path); return false; }
    bool ok = (len == 0) || (fwrite(data, 1, len, f) == len);
    fclose(f);
    if (!ok) logf_("  ECHEC fwrite %s (%u o)", path, len);
    return ok;
}

// Extracts a clean ROMFS path from a BCAT bundle path.
// Bundle paths come as:
//   directories.meta                          -> skip (metadata)
//   directories/<digest>/coopdata/Setting.byml -> "coopdata/Setting.byml"
//   coopdata/Setting.byml                     -> "coopdata/Setting.byml" (pass-through)
static const char *dataRelPath(const char *rel) {
    if (strcmp(rel, "directories.meta") == 0) return NULL;
    if (strcmp(rel, "files.meta") == 0) return NULL;
    if (strncmp(rel, "directories/", 12) == 0) {
        const char *slash = strchr(rel + 12, '/');
        if (slash && slash[1] != '\0') return slash + 1;
        return NULL;
    }
    if (strchr(rel, '/') != NULL) return rel;
    return NULL;
}

static bool writeBundleTo(const unsigned char *b, size_t len, const char *basePath) {
    if (len < 8 || memcmp(b, "NXBC", 4) != 0) { logf_("bundle: magic invalide"); return false; }
    u32 count;
    memcpy(&count, b + 4, 4);
    logf_("bundle: %u blobs", count);
    size_t off = 8;
    u32 written = 0;
    for (u32 i = 0; i < count; i++) {
        if (off + 2 > len) return false;
        u16 pl;
        memcpy(&pl, b + off, 2);
        off += 2;
        if (off + (size_t)pl + 4 > len) return false;
        char rel[FS_MAX_PATH];
        size_t n = (pl < sizeof(rel) - 1) ? pl : sizeof(rel) - 1;
        memcpy(rel, b + off, n);
        rel[n] = '\0';
        off += pl;
        u32 dl;
        memcpy(&dl, b + off, 4);
        off += 4;
        if (off + (size_t)dl > len) return false;
        // Securite : rejeter les traversees de repertoire et les chemins absolus.
        if (strstr(rel, "..") != NULL || rel[0] == '/' || strchr(rel, ':') != NULL || rel[0] == '\0') {
            logf_("  REJETE chemin invalide: \"%s\"", rel);
            return false;
        }

        const char *dataRel = dataRelPath(rel);
        if (!dataRel) {
            logf_("  ignore %s (meta/unknown)", rel);
            continue;
        }

        char path[FS_MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s", basePath, dataRel);
        if (!writeFileB(path, b + off, dl)) return false;
        logf_("  ok %s (%u o)", dataRel, dl);
        written++;
        off += dl;
    }
    logf_("bundle: %u fichiers ecrits sur %u blobs", written, count);
    return written > 0;
}

nextendo_bcat_result nextendo_bcat_install_s2(void) {
    g_log = fopen(LOG_PATH, "w");
    logf_("=== Nextendo BCAT install S2 (v4 — LayeredFS) ===");

    // Download the BCAT bundle to a temp file on SD card instead of buffering
    // in RAM. The Switch has limited heap (especially in applet mode) and the
    // doubling realloc in net_https_get can cause fragmentation at 4 MB.
    const char *tmpPath = "sdmc:/nextendo_bcat.bundle";
    FILE *tmpFile = fopen(tmpPath, "wb");
    if (!tmpFile) {
        logf_("ECHEC: impossible de creer le fichier temp %s", tmpPath);
        if (g_log) fclose(g_log);
        return NB_NET_FAIL;
    }

    int status = 0;
    sslInitialize(1);
    long bodyBytes = net_https_get_to_file(BCAT_HOST, BCAT_PATH, tmpFile, &status);
    sslExit();
    fclose(tmpFile);

    logf_("https: status=%d body=%ld o", status, bodyBytes);
    if (bodyBytes < 0) {
        remove(tmpPath);
        switch (status) {
            case NET_ERR_CONNECT:
                logf_("ECHEC: serveur %s:%d injoignable", BCAT_HOST, BCAT_PORT);
                if (g_log) fclose(g_log);
                return NB_NET_CONNECT;
            case NET_ERR_TIMEOUT:
                logf_("ECHEC: timeout reponse %s:%d", BCAT_HOST, BCAT_PORT);
                if (g_log) fclose(g_log);
                return NB_NET_TIMEOUT;
            case NET_ERR_PROTO:
                logf_("ECHEC: reponse HTTPS invalide depuis %s:%d", BCAT_HOST, BCAT_PORT);
                if (g_log) fclose(g_log);
                return NB_NET_HTTP_ERR;
            default:
                logf_("ECHEC: erreur reseau %d (serveur %s:%d)", status, BCAT_HOST, BCAT_PORT);
                if (g_log) fclose(g_log);
                return NB_NET_FAIL;
        }
    }
    if (status == 204) {
        remove(tmpPath);
        logf_("204 : rien de publie");
        if (g_log) fclose(g_log);
        return NB_NO_SCHEDULE;
    }
    if (status != 200 || bodyBytes < 8) {
        logf_("ECHEC: status HTTP %d attendu 200, body=%ld o", status, bodyBytes);
        remove(tmpPath);
        if (g_log) fclose(g_log);
        return NB_NET_HTTP_ERR;
    }

    // Read the temp file back into a single exact-size allocation.
    size_t blen = (size_t)bodyBytes;
    unsigned char *bundle = (unsigned char *)malloc(blen);
    if (!bundle) {
        logf_("ECHEC: malloc(%zu) impossible", blen);
        remove(tmpPath);
        if (g_log) fclose(g_log);
        return NB_NET_FAIL;
    }
    tmpFile = fopen(tmpPath, "rb");
    if (!tmpFile || fread(bundle, 1, blen, tmpFile) != blen) {
        logf_("ECHEC: lecture du fichier temp");
        if (tmpFile) fclose(tmpFile);
        free(bundle);
        remove(tmpPath);
        if (g_log) fclose(g_log);
        return NB_NET_FAIL;
    }
    fclose(tmpFile);
    remove(tmpPath);
    logf_("bundle: %zu o lus du fichier temp", blen);

    const char *regionIds[] = { S2_TITLE_ID_USA, S2_TITLE_ID_EUR };
    bool anyOk = false;
    bool anyErr = false;

    for (int r = 0; r < 2; r++) {
        char base[FS_MAX_PATH];
        snprintf(base, sizeof(base), LAYEREDFS_BASE, regionIds[r]);

        logf_("--- region %s ---", regionIds[r]);
        clearLayeredFS(base);

        if (writeBundleTo(bundle, blen, base)) {
            logf_("region %s: OK", regionIds[r]);
            anyOk = true;
        } else {
            logf_("region %s: ECHEC", regionIds[r]);
            anyErr = true;
        }
    }

    free(bundle);

    logf_("=== resultat: %s ===", anyOk ? "OK" : "ECHEC");
    if (g_log) { fclose(g_log); g_log = NULL; }

    if (anyOk) return NB_OK;
    if (anyErr) return NB_WRITE_FAIL;
    return NB_BAD_BUNDLE;
}
