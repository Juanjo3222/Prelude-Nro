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
//  Nextendo .nro — auto-mise a jour via GitHub Releases.
//  Utilise le service httpc de la Switch (HTTPS natif) pour
//  interroger directement l'API GitHub, sans passer par le VPS.
// ============================================================
#include <switch.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include "nextendo_update.h"

#define GH_API_URL  "https://api.github.com/repos/Juanjo3222/Prelude-Nro/releases/latest"
#define NRO_PATH    "sdmc:/switch/nextendo.nro"
#define NRO_TMP     "sdmc:/switch/nextendo.nro.new"

// Extrait une valeur string JSON "key":"value"
static bool json_str(const unsigned char *b, size_t len, const char *key, char *out, size_t out_max) {
    size_t kl = strlen(key);
    for (size_t i = 0; i + kl < len; i++) {
        if (memcmp(b + i, key, kl) == 0) {
            size_t j = i + kl;
            while (j < len && (b[j] == ' ' || b[j] == ':' || b[j] == '"')) j++;
            size_t start = j;
            while (j < len && b[j] != '"') j++;
            size_t slen = j - start;
            if (slen > 0 && slen < out_max) {
                memcpy(out, b + start, slen);
                out[slen] = '\0';
                return true;
            }
        }
    }
    return false;
}

// GET HTTP via httpc (HTTPS natif). Renvoie le body malloc ou NULL.
static Result httpc_get(const char* url, unsigned char** out_buf, size_t* out_len, int* out_status) {
    *out_buf = NULL; *out_len = 0;
    if (out_status) *out_status = 0;

    Result rc = httpcInitialize();
    if (R_FAILED(rc)) return rc;

    HttpcContext ctx;
    rc = httpcOpenContext(&ctx, HttpcRequestMethod_Get, url, 0);
    if (R_FAILED(rc)) { httpcExit(); return rc; }

    httpcAddRequestHeaderField(&ctx, "Accept", "application/vnd.github+json");
    httpcAddRequestHeaderField(&ctx, "User-Agent", "NextendoHomebrew");

    rc = httpcBeginRequest(&ctx);
    if (R_FAILED(rc)) { httpcCloseContext(&ctx); httpcExit(); return rc; }

    u32 status = 0;
    rc = httpcGetResponseStatusCode(&ctx, &status, 0);
    if (R_FAILED(rc)) { httpcCloseContext(&ctx); httpcExit(); return rc; }
    if (out_status) *out_status = (int)status;

    u32 content_size = 0;
    httpcGetDownloadSizeState(&ctx, NULL, &content_size);

    size_t cap = content_size > 0 ? content_size + 1 : (1 << 16);
    size_t len = 0;
    unsigned char* buf = malloc(cap);
    if (!buf) { httpcCloseContext(&ctx); httpcExit(); return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory); }

    u32 read = 0;
    while (1) {
        if (len + 8192 > cap) {
            cap *= 2;
            unsigned char* nb = realloc(buf, cap);
            if (!nb) { free(buf); httpcCloseContext(&ctx); httpcExit(); return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory); }
            buf = nb;
        }
        rc = httpcReceiveData(&ctx, buf + len, cap - len, &read);
        if (rc == HTTPC_RESULTCODE_DOWNLOADPENDING) continue;
        if (R_FAILED(rc) || read == 0) break;
        len += read;
    }

    httpcCloseContext(&ctx);
    httpcExit();

    *out_buf = buf;
    *out_len = len;
    return 0;
}

// GET HTTP via httpc avec streaming direct vers FILE
static Result httpc_get_to_file(const char* url, FILE* f, int* out_status) {
    if (out_status) *out_status = 0;

    Result rc = httpcInitialize();
    if (R_FAILED(rc)) return rc;

    HttpcContext ctx;
    rc = httpcOpenContext(&ctx, HttpcRequestMethod_Get, url, 0);
    if (R_FAILED(rc)) { httpcExit(); return rc; }

    httpcAddRequestHeaderField(&ctx, "Accept", "application/octet-stream");
    httpcAddRequestHeaderField(&ctx, "User-Agent", "NextendoHomebrew");

    rc = httpcBeginRequest(&ctx);
    if (R_FAILED(rc)) { httpcCloseContext(&ctx); httpcExit(); return rc; }

    u32 status = 0;
    rc = httpcGetResponseStatusCode(&ctx, &status, 0);
    if (R_FAILED(rc)) { httpcCloseContext(&ctx); httpcExit(); return rc; }
    if (out_status) *out_status = (int)status;
    if (status != 200) { httpcCloseContext(&ctx); httpcExit(); return MAKERESULT(255, 1); }

    char buf[32768];
    u32 read = 0;
    while (1) {
        rc = httpcReceiveData(&ctx, buf, sizeof(buf), &read);
        if (rc == HTTPC_RESULTCODE_DOWNLOADPENDING) continue;
        if (R_FAILED(rc) || read == 0) break;
        if (fwrite(buf, 1, read, f) != read) {
            httpcCloseContext(&ctx); httpcExit(); return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }
    }

    httpcCloseContext(&ctx);
    httpcExit();
    return 0;
}

NextendoUpdate nextendo_update_check(void) {
    NextendoUpdate u = { false, 0, 0 };
    unsigned char *body = NULL;
    size_t len = 0;
    int status = 0;

    Result rc = httpc_get(GH_API_URL, &body, &len, &status);
    if (R_SUCCEEDED(rc) && body && status == 200) {
        char tag[32] = {0};
        if (json_str(body, len, "\"tag_name\"", tag, sizeof(tag))) {
            // Si le tag est different de la version actuelle, mise a jour dispo
            // Version actuelle : v2.0.8 (build 27)
            if (strcmp(tag, "v2.0.8") != 0) {
                u.available = true;
                // Extrait un numero approximatif pour l'affichage
                long ver = 0;
                for (size_t i = 0; i < strlen(tag); i++) {
                    if (tag[i] >= '0' && tag[i] <= '9') ver = ver * 10 + (tag[i] - '0');
                }
                u.latest = (int)ver;
            }
        }
    }
    if (body) free(body);
    return u;
}

nextendo_update_result nextendo_update_apply(long expectedSize) {
    FILE *f = fopen(NRO_TMP, "wb");
    if (!f) {
        mkdir("sdmc:/switch", 0777);
        f = fopen(NRO_TMP, "wb");
    }
    if (!f) return NUP_WRITE_FAIL;

    unsigned char *body = NULL;
    size_t len = 0;
    int status = 0;
    Result rc = httpc_get(GH_API_URL, &body, &len, &status);

    char downloadUrl[512] = {0};
    if (R_SUCCEEDED(rc) && body && status == 200) {
        // Cherche browser_download_url pour nextendo.nro
        for (size_t i = 0; i + 22 < len; i++) {
            if (memcmp(body + i, "browser_download_url", 20) == 0) {
                size_t j = i + 20;
                while (j < len && (body[j] == ' ' || body[j] == ':' || body[j] == '"')) j++;
                size_t start = j;
                while (j < len && body[j] != '"') j++;
                size_t urlLen = j - start;
                if (urlLen > 0 && urlLen < sizeof(downloadUrl) - 1) {
                    memcpy(downloadUrl, body + start, urlLen);
                    downloadUrl[urlLen] = '\0';
                    if (strstr(downloadUrl, "nextendo.nro")) break;
                }
            }
        }
    }
    if (body) free(body);

    if (downloadUrl[0] == '\0') { fclose(f); remove(NRO_TMP); return NUP_NET_FAIL; }

    int dl_status = 0;
    rc = httpc_get_to_file(downloadUrl, f, &dl_status);
    fclose(f);

    if (R_FAILED(rc) || dl_status != 200) { remove(NRO_TMP); return NUP_NET_FAIL; }
    
    struct stat st;
    if (stat(NRO_TMP, &st) == 0) {
        if (expectedSize > 0 && st.st_size != expectedSize) { remove(NRO_TMP); return NUP_SIZE_FAIL; }
    } else {
        remove(NRO_TMP); return NUP_NET_FAIL;
    }
    
    fsdevCommitDevice("sdmc");

    remove(NRO_PATH);
    if (rename(NRO_TMP, NRO_PATH) != 0) {
        FILE *src = fopen(NRO_TMP, "rb");
        if (!src) { remove(NRO_TMP); return NUP_WRITE_FAIL; }
        FILE *dst = fopen(NRO_PATH, "wb");
        if (!dst) { fclose(src); remove(NRO_TMP); return NUP_WRITE_FAIL; }
        char cbuf[16384];
        size_t n;
        bool ok = true;
        while ((n = fread(cbuf, 1, sizeof(cbuf), src)) > 0)
            if (fwrite(cbuf, 1, n, dst) != n) { ok = false; break; }
        fclose(src); fclose(dst);
        remove(NRO_TMP);
        if (!ok) return NUP_WRITE_FAIL;
    }

    fsdevCommitDevice("sdmc");
    return NUP_OK;
}
