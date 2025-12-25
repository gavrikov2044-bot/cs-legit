#!/bin/bash
# Admin Panel - Quick Access Script for macOS

SERVER="single-project.duckdns.org"
SERVER_PASS="mmE28jaX99"
LOCAL_PORT="8080"

echo "🔐 CS-Legit Admin Panel"
echo "========================"
echo ""

# Check if sshpass is installed
if ! command -v sshpass &> /dev/null; then
    echo "⚠️  sshpass не установлен. Устанавливаю..."
    brew install hudochenkov/sshpass/sshpass 2>/dev/null || {
        echo "Установи Homebrew и sshpass:"
        echo "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
        echo "  brew install hudochenkov/sshpass/sshpass"
        echo ""
        echo "Или введи пароль вручную: $SERVER_PASS"
        ssh -L $LOCAL_PORT:127.0.0.1:80 root@$SERVER
        exit 0
    }
fi

# Kill any existing tunnel on port 8080
lsof -ti:$LOCAL_PORT | xargs kill 2>/dev/null

echo "🔗 Подключаюсь к серверу..."
sshpass -p "$SERVER_PASS" ssh -o StrictHostKeyChecking=no -L $LOCAL_PORT:127.0.0.1:80 -N root@$SERVER &
SSH_PID=$!

sleep 2

# Check if tunnel is working
if ! lsof -i:$LOCAL_PORT > /dev/null 2>&1; then
    echo "❌ Ошибка подключения!"
    exit 1
fi

echo "✅ Туннель открыт!"
echo ""
echo "📌 Данные для входа:"
echo "   HTTP Auth: admin / SuperAdmin123"
echo "   Login: admin / admin123"
echo ""

# Open browser with credentials
open "http://admin:SuperAdmin123@localhost:$LOCAL_PORT/panel/"

echo "🌐 Браузер открыт!"
echo ""
echo "Нажми Ctrl+C чтобы закрыть туннель..."
echo ""

# Wait for SSH to finish (keep tunnel open)
wait $SSH_PID

