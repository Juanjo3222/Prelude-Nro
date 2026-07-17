# Penne Presence Chain — Server Handler Specifications

Based on real mitmproxy capture from a stock Switch (FW 22.5.0) using device.pem client certificate.

---

## 1. DAUTH — Challenge

**Already implemented?** Check if your server proxies or handles dauth-lp1.ndas.srv.nintendo.net.

```
POST https://dauth-lp1.ndas.srv.nintendo.net/v8/challenge
Content-Type: application/x-www-form-urlencoded

key_generation=22

→ 200 OK
Content-Type: application/json
{
  "challenge": "<base64 challenge string>",
  "data": "<base64 data string>"
}
```

---

## 2. DAUTH — Device Auth Tokens

```
POST https://dauth-lp1.ndas.srv.nintendo.net/v8/device_auth_tokens
Content-Type: application/json

{
  "system_version": "00160500",
  "fw_revision": "<sha1 hex>",
  "ist": false,
  "token_requests": [
    { "client_id": "<16-char hex>" },
    ...
  ],
  "key_generation": 22,
  "challenge": "<from /v8/challenge response>",
  "mac": "<base64 mac>"
}

→ 200 OK
Content-Type: application/json
{
  "results": [
    {
      "client_id": "<16-char hex>",
      "device_auth_token": "<JWT>",
      "expires_in": 86400
    },
    ...
  ]
}
```

---

## 3. DAUTH — Edge Tokens

```
POST https://dauth-lp1.ndas.srv.nintendo.net/v8/edge_tokens
Content-Type: application/json

{
  "system_version": "00160500",
  "fw_revision": "<sha1 hex>",
  "ist": false,
  "token_requests": [
    {
      "client_id": "<16-char hex>",
      "vendor_id": "akamai"
    },
    ...
  ],
  "key_generation": 22,
  "challenge": "<base64>",
  "mac": "<base64>"
}

→ 200 OK
Content-Type: application/json
{
  "results": [
    {
      "client_id": "<16-char hex>",
      "vendor_id": "akamai|fastly",
      "dtoken": "<akamai or fastly token string>",
      "expires_in": 86400
    },
    ...
  ]
}
```

---

## 4. BEACH — Penne ID Registration ← **THE KEY ENDPOINT**

This is what the console calls instead of god/v1/penne_ids. The penne_id is **generated locally by the console** and stored in `penne_persistent.bin`. The console simply registers it here.

```
POST https://beach.hac.lp1.eshop.nintendo.net/v1/devices/<deviceId>/penne_id/register
User-Agent: NintendoSDK Firmware/22.5.0-1.0 (platform:NX; did:<deviceId>; eid:lp1)
Accept: application/json
X-DeviceAuthorization: Bearer <JWT>
Content-Type: application/json

{
  "penne_id": "pXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
}

→ 200 OK
Content-Length: 0
(empty body)
```

**Implementation notes:**
- `<deviceId>` is the console's device ID (e.g. `DEVICEID00000000` format)
- `penne_id` format: starts with `p`, followed by 32 lowercase alphanumeric chars (total 33 chars)
- The JWT in `X-DeviceAuthorization` comes from dauth but you can accept any valid token or skip validation for now
- **Response is 200 with Content-Length: 0 and EMPTY BODY** — do NOT return JSON, literally empty

---

## 5. VERMILLION — Get Vermillion Device ID

```
GET https://gw.hac.lp1.vermillion.srv.nintendo.net/v1/devices/<vermillion-device-id>
Accept: */*
X-Nintendo-Device-Authorization: Bearer <JWT>

(no body)

→ 200 OK
Content-Type: application/json
Content-Length: 50

{
  "vermillionDeviceId": "<base64 string>"
}
```

**Note:** The `<vermillion-device-id>` in the URL path is the device ID that was assigned in a previous `/v1/devices/initialize` call (see below). This endpoint is used to **retrieve** the console's vermillion device ID.

---

## 6. VERMILLION — Device Initialize

```
POST https://gw.hac.lp1.vermillion.srv.nintendo.net/v1/devices/initialize
Accept: */*
X-Nintendo-Device-Authorization: Bearer <JWT>
Content-Type: application/json

{
  "penneId": "pXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
}

→ 204 No Content
(empty body)
```

**Implementation notes:**
- Takes the penneId from the beach registration
- **Response is 204 No Content with NO body** — not 200, specifically 204
- The server should generate/store a vermillion device ID associated with this penneId (used by the GET endpoint above)

---

## 7. VAL — Login Ticket

```
POST https://val.hac.lp1.penne.srv.nintendo.net/v1/login_tickets
User-Agent: libcurl (nnPenne; <uuid>; SDK 22.2.0.0; Add-on 22.2.0.0)
Accept: */*
Content-Type: application/json
Authorization: Bearer <JWT>
X-NPC: 0
X-Login-Try: 0
X-Fro-Result: 000-0000
X-Tolerant: true

{
  "id": "pXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
  "password": "<32-char password>"
}

→ 200 OK
Content-Type: application/json
Content-Length: 411

{
  "expires_at": 1784646213,
  "frontline_fqdn": "fro-1.hac.lp1.penne.srv.nintendo.net",
  "issued_at": 1784300613,
  "persistent_connection_params_simple": {
    "awake": {
      "count": 2,
      "disable": false,
      "idle": 60,
      "interval": 10
    },
    "ignore_rst": true,
    "retry_count": 2,
    "rtt_max": 1000,
    "sleep": {
      "count": 1440,
      "disable": true,
      "idle": 60,
      "interval": 10
    },
    "wait_sec": 3600,
    "wowl_timeout": 200
  },
  "ticket": "2f44f82a05d70258e88e004bf346fce9f44ed6d90678832b"
}
```

**Implementation notes:**
- `id` is the penne_id (`p` + 32 chars)
- `password` is 32 characters (unknown format, stored locally on console alongside penne_id)
- Custom headers `X-NPC`, `X-Login-Try`, `X-Fro-Result`, `X-Tolerant` must be accepted
- `ticket` is a hex string (used for the persistent connection)
- `frontline_fqdn` should point to your frontline server
- `persistent_connection_params_simple` values can be hardcoded from the capture above
- `expires_at` = `issued_at` + 345600 (4 days) approximately, but you can set any reasonable future timestamp
- **For now**: `frontline_fqdn` can be set to a fake but valid FQDN (e.g. `fro-1.hac.lp1.penne.srv.nintendo.net`), or to your own frontline server if you implement one

---

## 8. VAL — Frontlines

```
GET https://val.hac.lp1.penne.srv.nintendo.net/v1/frontlines
User-Agent: libcurl (nnPenne; <uuid>; SDK 22.2.0.0; Add-on 22.2.0.0)
Accept: */*
Content-Type: application/json
Authorization: Bearer <JWT>
X-NPC: 0
X-Login-Try: 0
X-Fro-Result: 000-0000
X-Tolerant: true

(no body)

→ 200 OK
Content-Type: application/json
Content-Length: 330

{
  "current_time": 1784299496,
  "frontline_fqdn": "fro-1.hac.lp1.penne.srv.nintendo.net",
  "persistent_connection_params_simple": {
    "awake": {
      "count": 2,
      "disable": false,
      "idle": 60,
      "interval": 10
    },
    "ignore_rst": true,
    "retry_count": 2,
    "rtt_max": 1000,
    "sleep": {
      "count": 1440,
      "disable": true,
      "idle": 60,
      "interval": 10
    },
    "wait_sec": 3600,
    "wowl_timeout": 200
  }
}
```

**Implementation notes:**
- `current_time` is current unix timestamp
- Same structure as login_tickets' `persistent_connection_params_simple` but without `issued_at`, `expires_at`, `ticket`
- This endpoint returns the current frontline server assignment

---

## 9. GOD — Penne IDs (Inferred)

The real console does **NOT** call this endpoint during the presence chain. However, based on the wiki and pattern from `/v1/login_tickets`, this endpoint is expected to exist and likely handles penne_id registration/authentication.

```
POST https://god.hac.lp1.penne.srv.nintendo.net/v1/penne_ids
Content-Type: application/json

{
  "id": "pXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
  "password": "<32-char password>"
}

→ 200 OK
(empty body)
```

**Note:** If the console never hits this endpoint (confirmed: it uses beach register instead), your server still needs to accept it in case other implementations or debug flows call it. Accept `{"id": "...", "password": "..."}`, validate, return 200 empty.

---

## Summary: Minimal Implementation Priority

| Priority | Endpoint | Status | Why |
|----------|----------|--------|-----|
| **P0** | `beach POST /v1/devices/<id>/penne_id/register` | ⬜ Not implemented | Blocks errors 2123-0011 / 2810-1224 |
| **P0** | `val POST /v1/login_tickets` | ⬜ Not implemented | Required for Penne auth |
| **P0** | `val GET /v1/frontlines` | ⬜ Not implemented | Required for Penne connection params |
| **P1** | `vermillion POST /v1/devices/initialize` | ⬜ Not implemented | Part of chain after beach |
| **P1** | `vermillion GET /v1/devices/<id>` | ⬜ Not implemented | May be needed |
| **P2** | `god POST /v1/penne_ids` | ⬜ Not implemented | Fallback/inferred endpoint |

---

## Critical Response Rules

1. **beach `penne_id/register`** → `200 OK`, **EMPTY BODY** (`Content-Length: 0`), no JSON
2. **vermillion `devices/initialize`** → `204 No Content`, **EMPTY BODY**, no JSON
3. All other endpoints → standard JSON responses as captured above
4. Headers from the request should be echoed back where Nintendo normally does (see captured responses for pattern)
