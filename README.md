# Hawata — Smart Port Fishing

[![Build Status](https://img.shields.io/badge/build-pending-lightgrey)](https://example.com/build)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

## Description
Hawata is a desktop application built with Qt to help harbor staff manage port operations for a smart fishing port. The application centralizes dock management, boat tracking, fish product inventory, user/role management, and partner companies. It features a compact dashboard, CRUD interfaces for core entities, reporting/export options, and an integrated chat assistant that can query the database or fall back to a local AI service. Hawata is designed to run against an Oracle database (preferred) and supports ODBC fallbacks for environments where Oracle clients or plugins are not available.

## Technologies Used
Frontend: Qt Widgets (C++), QML where used
Backend: C++ (Qt framework — Sql, Network, Widgets modules), CMake build system
Database: Oracle (QOCI via Qt) with QODBC fallback; SQL scripts provided for schema

## Prerequisites
- Node.js 18+ (optional — used only for auxiliary tooling or demo scripts)
- Docker (optional)
- Qt 6.x (with `Sql`, `Widgets`, and `Network` modules)
- Oracle Instant Client (or an appropriate ODBC driver installed)
- CMake 3.16+ and a C++17-compatible compiler (MSVC / MinGW / clang)

## Installation
Install project dependencies and build the app (Qt and Oracle/ODBC must be installed separately):

```bash
# from repository root
# create build directory
mkdir build && cd build
# configure with CMake
cmake ..
# build (use --config Release on Windows)
cmake --build . --config Release
```

If you use any Node-based auxiliary tooling in `tools/` or a demo folder, run:

```bash
npm install
```

## Quick Start
Follow one of the quick start paths depending on whether you want to run the native Qt app or the Node demo server.

Native (Qt/C++):

```bash
# Build and run (recommended)
mkdir build && cd build
cmake ..
cmake --build . --config Release
./Hawata   # or .\build\Release\Hawata.exe on Windows
```

Node demo (if present):

```bash
npm install
npm start
# open http://localhost:3000 or the port shown by the demo server
```

## Launch
Run the built executable from the build output. Examples:

Unix/macOS:
```bash
./Hawata
```

Windows (from build\Release or run via Qt Creator):
```powershell
.\build\Release\Hawata.exe
```

If a Node demo server is present, start it with:
```bash
npm start
```

## Environment Variables
See `.env.example` for a machine-readable template. Common variables used by the application:

- `HWT_DB_USER` — database username (default: `qtuser`)
- `HWT_DB_PASSWORD` — database password
- `HWT_DB_HOST` — database host (default: `localhost`)
- `HWT_DB_PORT` — database port (default: `1521`)
- `HWT_DB_SERVICE` — database service name or SID (default: `XE`)
- `HWT_DB_DSN` — ODBC DSN fallback (default: `HWT`)
- `HWT_DB_ODBC_CONN` — full ODBC connection string (optional)
- `HWT_ORACLE_CLIENT_DIR` — Oracle Instant Client folder to prepend to `PATH` (optional)
- `OLLAMA_HOST` — (optional) local Ollama host (default: `http://localhost:11434`) for AI fallback

Note: The application tries an ODBC DSN named by `HWT_DB_DSN` if QOCI is not available. On Windows, ensure that the correct (32/64-bit) ODBC driver and DSN exist for the Qt runtime you use.

## Demo
Video: https://...

Deployment: https://...

## Authors
Name --- Class --- Year --- Supervisor: ...

## CONTRIBUTING

Contributions are welcome. To contribute:

- Fork the repository and create a feature branch.
- Open a pull request with a clear description of the change.
- Ensure the project builds and add tests or documentation where appropriate.

Before submitting, make sure the repo does not include secrets (passwords, API keys, or full `.env` files). Use `.env.example` for configuration samples.


---

For developer notes, database schema and per-feature documentation see: `PRODUCT_CRUD_SETUP.md`, `database_schema.sql`, and additional docs in the repository.
