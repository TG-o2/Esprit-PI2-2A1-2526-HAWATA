# Hawata Port Management - Installation Guide

## Step 1: Install Required Software
- Qt 6.7.3 with MinGW 64-bit
- Oracle Database (or Oracle XE)
- Oracle Instant Client

## Step 2: Set Up the Database
```bash
# Connect to Oracle as HWT user
sqlplus hwt/your_password

# Run the database setup script
@database_setup.sql