# Hawata — Smart Port Fishing

[![Build Status](https://img.shields.io/badge/build-pending-lightgrey)](https://example.com/build)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

## Description
Hawata is a desktop application built with Qt to help harbor staff manage port operations for a smart fishing port. The application centralizes dock management, boat tracking, fish product inventory, user/role management, and partner companies. It features a compact dashboard, CRUD interfaces for core entities, reporting/export options, and an integrated chat assistant that can query the database or fall back to a local AI service. Hawata is designed to run against an Oracle database (preferred) and supports ODBC fallbacks for environments where Oracle clients or plugins are not available.

## Technologies Used
- **Frontend:** Qt Widgets (C++), QML where used
- **Backend:** C++ (Qt framework — Sql, Network, Widgets modules), CMake build system
- **Database:** Oracle (QOCI via Qt) with QODBC fallback; SQL scripts provided for schema

## Prerequisites
- **Qt 6.7.3** (MinGW 64-bit) with Sql, Widgets, and Network modules
- **Oracle Database** (XE is free) or Oracle Instant Client with ODBC driver
- **CMake 3.16+** and a C++17-compatible compiler (MSVC / MinGW / clang)
- Node.js 18+ (optional — used only for auxiliary tooling or demo scripts)
- Docker (optional)

---

## Installation Guide

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
