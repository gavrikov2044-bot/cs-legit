#!/bin/bash
# Simple Admin Panel Access (manual password entry)

echo "🔐 CS-Legit Admin Panel"
echo "========================"
echo ""
echo "Введи пароль: mmE28jaX99"
echo ""

# Kill existing tunnel
lsof -ti:8080 | xargs kill 2>/dev/null

# Open browser after 3 seconds
(sleep 3 && open "http://admin:SuperAdmin123@localhost:8080/panel/") &

# Connect with manual password
ssh -L 8080:127.0.0.1:80 root@single-project.duckdns.org
