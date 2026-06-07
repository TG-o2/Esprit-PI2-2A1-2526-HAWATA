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

## Installation Guide

### Step 0: Extract the Project Files
1. Locate the downloaded zip file (e.g., `Hawata.zip`)
2. Right-click the zip file
3. Select **Extract All...**
4. Choose a destination folder (e.g., `C:\Users\YourName\Desktop\Hawata`)
5. Click **Extract**
6. Open the extracted folder

> ⚠️ **IMPORTANT:** Do NOT run commands from inside the zip file. You must extract first!

### Step 1: Install Oracle Database
### Step 2: Install Oracle Instant Client
### Step 3: Install Qt and CMake
### Step 4: Set Up the Database
```bash
# Connect to Oracle as SYSTEM and create HWT user
sqlplus system/your_password

CREATE USER HWT IDENTIFIED BY "your_password";
GRANT CONNECT, RESOURCE, CREATE SESSION, CREATE TABLE, CREATE SEQUENCE TO HWT;
GRANT UNLIMITED TABLESPACE TO HWT;
ALTER USER HWT ACCOUNT UNLOCK;
EXIT;

# Connect as HWT and run the setup script
sqlplus hwt/your_password
@database_setup.sql
EXIT;
Step 5: Configure Database Connection
Open connection.cpp and change these lines:

cpp
db.setDatabaseName("XE");
db.setUserName("HWT");
db.setPassword("your_password");
Step 6: Build and Run
bash
# Navigate to the extracted project folder
cd C:\Users\YourName\Desktop\Hawata

# Create build folder and build
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build . --config Release
.\Release\Sign_up.exe   # Windows
# or ./Hawata on Mac/Linux
Step 7: Login
Email: admin@hawata.com

Password: admin123

Quick Start
bash
# Build and run the application
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build . --config Release
.\Release\Sign_up.exe   # Windows
Launch
Run the built executable from the build output:

Windows (from build\Release or run via Qt Creator):

powershell
.\build\Release\Sign_up.exe
Unix/macOS:

bash
./Hawata


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
Video: https:[//...](https://youtu.be/V0Z_-v9EziU)

Deployment:not applicable

## Authors
Chaima Khayati ---2A1 --- Year 2 --- Supervisor: Saoussen Lakhdhar
Ahmed Ben Khelifa ---2A1 --- Year 2 --- Supervisor: Saoussen Lakhdhar
Fatma ezaahra joobeur ---2A1 --- Year 2 --- Supervisor: Saoussen Lakhdhar
Tasnim Guesmi ---2A1 --- Year 2 --- Supervisor: Saoussen Lakhdhar
Tasnim Guesmi ---2A1 --- Year 2 --- Supervisor: Saoussen Lakhdhar
Jeridi Jihene ---2A1 --- Year 2 --- Supervisor:Saoussen Lakhdhar
## CONTRIBUTING

Contributions are welcome. To contribute:

- Fork the repository and create a feature branch.
- Open a pull request with a clear description of the change.
- Ensure the project builds and add tests or documentation where appropriate.

Before submitting, make sure the repo does not include secrets (passwords, API keys, or full `.env` files). Use `.env.example` for configuration samples.


---

For developer notes, database schema and per-feature documentation see: `PRODUCT_CRUD_SETUP.md`, `database_schema.sql`, and additional docs in the repository.


