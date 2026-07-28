// Prelude -- Nintendo Switch homebrew for the Nextendo Network.
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
//  Nextendo .nro -- auto-update check via HTTP (lightweight, no SSL),
//  actual download via HTTPS (GitHub Releases).
//  The startup check uses plain HTTP to avoid sslInitialize/sslExit
//  side effects on the system SSL service — which can cause
//  "Función no disponible" on login after exiting Prelude.
// ============================================================
#include <switch.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include "nextendo_update.h"
#include "nextendo_net.h"

// Lightweight HTTP endpoint for startup version check (no SSL needed).
// Run your own or use any plain-HTTP endpoint that returns {"version":N,"size":M}.
#define VPS_IP       "51.178.29.194"
#define VPS_PORT     8095
#define VPS_LATEST   "/api/nro/latest"

// GitHub download: URL constructed from build number.
#define GH_RELEASE   "https://github.com/Juanjo3222/Prelude-Nro/releases/download/v2.0.%d/nextendo.nro"

#define NRO_PATH     "sdmc:/switch/nextendo.nro"
#define NRO_TMP      "sdmc:/switch/nextendo.nro.new"

// Stored by nextendo_update_check, consumed by nextendo_update_apply.
// Static because the apply function signature doesn't include the URL.
static char g_download_url[512] = {0};

// Parse a JSON long value: ,"key":NNN
static long json_long(const unsigned char *b, size_t len, const char *key) {
    size_t kl = strlen(key);
    for (size_t i = 0; i + kl < len; i++) {
        if (memcmp(b + i, key, kl) == 0) {
            size_t j = i + kl;
            while (j < len && (b[j] == ' ' || b[j] == ':' || b[j] == '"')) j++;
            long v = 0;
            if (j < len && b[j] == '-') { j++; }
            while (j < len && b[j] >= '0' && b[j] <= '9') {
                v = v * 10 + (b[j] - '0'); j++;
            }
            return v;
        }
    }
    return -1;
}

// Check for update. Plain HTTP (no SSL) to a lightweight endpoint.
// If the endpoint is unreachable, no update is reported — graceful
// degradation. The VPS URL can be changed at build time.
// The actual download (nextendo_update_apply) uses HTTPS + sslInit.
NextendoUpdate nextendo_update_check(void) {
    NextendoUpdate u = { false, 0, 0 };
    socketInitializeDefault();

    size_t len = 0;
    int status = 0;
    unsigned char *body = net_http_get(VPS_IP, VPS_PORT, VPS_LATEST, &len, &status);

    if (body && status == 200) {
        long ver = json_long(body, len, "\"version\"");
        long sz  = json_long(body, len, "\"size\"");
        if (ver > NEXTENDO_BUILD && sz > 0) {
            u.available = true;
            u.latest = (int)ver;
            u.size = sz;
                    snprintf(g_download_url, sizeof(g_download_url), GH_RELEASE, (int)ver);
        }
        free(body);
    }

    socketExit();
    return u;
}

// Download and apply the update. Requires sslInitialize() before.
nextendo_update_result nextendo_update_apply(long expectedSize) {
    if (g_download_url[0] == '\0') return NUP_NET_FAIL;

    FILE *f = fopen(NRO_TMP, "wb");
    if (!f) {
        mkdir("sdmc:/switch", 0777);
        f = fopen(NRO_TMP, "wb");
    }
    if (!f) return NUP_WRITE_FAIL;

    socketInitializeDefault();
    Result rc = sslInitialize(4);
    if (R_FAILED(rc)) { fclose(f); socketExit(); return NUP_NET_FAIL; }

    char host[256] = {0};
    char path[1024] = {0};
    if (sscanf(g_download_url, "https://%255[^/]%1023s", host, path) < 2) {
        sslExit(); fclose(f); remove(NRO_TMP); socketExit(); return NUP_NET_FAIL;
    }

    int status = 0;
    long len = net_https_get_to_file(host, path, f, &status);
    fclose(f);
    sslExit();
    socketExit();

    if (len == -2) { remove(NRO_TMP); return NUP_WRITE_FAIL; }
    if (len < 0)   { remove(NRO_TMP); return NUP_NET_FAIL; }
    if (status != 200 || len < 4096) { remove(NRO_TMP); return NUP_NET_FAIL; }
    if (expectedSize > 0 && len != expectedSize) { remove(NRO_TMP); return NUP_SIZE_FAIL; }
    fsdevCommitDevice("sdmc");

    // Replace the old .nro (current runs from RAM, safe to overwrite).
    // rename() can fail on FAT32; fallback to copy.
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
