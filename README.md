<div align="center">
  <img src="https://capsule-render.vercel.app/api?type=rect&height=130&color=0:0ea5e9,100:22c55e&text=Asset%20Inventory%20Agent&fontSize=34&fontColor=ffffff&animation=fadeIn&fontAlignY=55" />
  <img src="https://readme-typing-svg.demolab.com?font=Fira+Code&size=14&duration=2200&pause=700&color=22C55E&center=true&vCenter=true&width=860&lines=Collect+Device+Info+%E2%86%92+Normalize+%E2%86%92+JSON+%E2%86%92+HTTP+POST;Server+validates+schema+%E2%86%92+stores+data+%E2%86%92+Dashboard+table+%2B+CSV+export;Retry+on+failure+%7C+Logs+without+breaking+main+flow" />
  <br/>
  <img src="https://skillicons.dev/icons?i=cpp&perline=1" />
</div>

---

## Deskripsi
**Asset Inventory Agent (C++)** adalah agent ringan untuk mengoleksi info perangkat (hardware/software basic) lalu mengirim **JSON** ke server via **HTTP**. Server memvalidasi schema JSON, menyimpan data, dan menyediakan **dashboard** sederhana + **export CSV** untuk audit/monitoring aset.

---

## Struktur Proyek
```text
cpp/
├─ README.md
├─ CMakeLists.txt
├─ src/
│  ├─ agent_main.cpp
│  ├─ server_main.cpp
│  ├─ inventory.cpp
│  ├─ inventory.hpp
│  ├─ platform.cpp
│  ├─ platform.hpp
│  ├─ mini_json.cpp
│  ├─ mini_json.hpp
│  ├─ http_client.cpp
│  ├─ http_client.hpp
│  ├─ http_server.cpp
│  ├─ http_server.hpp
│  ├─ file_store.cpp
│  ├─ file_store.hpp
│  ├─ logger.cpp
│  └─ logger.hpp
├─ assets/
│  ├─ preview_sent.json
│  ├─ dashboard_preview.png
│  └─ dashboard_preview.txt
├─ data/
│  └─ assets.jsonl
└─ logs/
   └─ app.log
```

---

## Preview
- JSON contoh yang dikirim agent: `assets/preview_sent.json`
- Preview dashboard daftar aset: `assets/dashboard_preview.png` (juga ada versi text: `assets/dashboard_preview.txt`)

---

## Build & Run (ringkas)
1) Jalankan server:
- `./asset_server 8080`
2) Jalankan agent:
- `./asset_agent --host 127.0.0.1 --port 8080 --path /api/assets --retries 3 --timeout 2000`
3) Buka dashboard:
- `http://localhost:8080/`

---

## Catatan Reliability
- Agent melakukan retry (1s → 2s → 4s) saat koneksi gagal.
- Jika gagal total, agent menulis log warning dan tetap exit 0 (agar tidak memutus proses utama/scheduler).
- Server menolak payload yang schema-nya tidak valid (HTTP 400 + detail).
