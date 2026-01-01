# 🚀 Deployment Guide

## Выбор хостинга

### Рекомендуемые VPS провайдеры

| Провайдер | Цена | Плюсы | Минусы |
|-----------|------|-------|--------|
| **Hetzner** | €4-20/мес | Дешёво, быстро, Европа | Нужна верификация |
| **OVH** | €5-30/мес | Без жалоб, надёжно | Медленная поддержка |
| **DigitalOcean** | $6-24/мес | Простой UI, документация | Дороже |
| **Vultr** | $5-20/мес | Много локаций | - |
| **Contabo** | €5-15/мес | Очень дёшево | Иногда тормозит |

### Минимальные требования
- 1 vCPU
- 2 GB RAM
- 20 GB SSD
- Ubuntu 22.04 LTS

## Установка на сервер

### 1. Подготовка сервера

```bash
# Обновление системы
sudo apt update && sudo apt upgrade -y

# Установка необходимого ПО
sudo apt install -y nodejs npm nginx certbot python3-certbot-nginx

# Установка Node.js 18+
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt install -y nodejs
```

### 2. Создание пользователя

```bash
# Создаём пользователя для сервиса
sudo useradd -m -s /bin/bash launcher
sudo passwd launcher

# Переключаемся на пользователя
sudo su - launcher
```

### 3. Клонирование и установка

```bash
# Клонируем репозиторий
git clone https://github.com/your-repo/launcher-server.git
cd launcher-server/backend

# Устанавливаем зависимости
npm install --production

# Создаём конфигурацию
cp .env.example .env
nano .env  # Редактируем настройки
```

### 4. Настройка .env

```env
PORT=3000
HOST=127.0.0.1
JWT_SECRET=ваш-очень-длинный-секретный-ключ
ENCRYPTION_KEY=ваш-32-байтный-ключ-шифрования
STORAGE_PATH=../storage
ADMIN_PASSWORD=сложный-пароль-админа
```

### 5. Инициализация БД

```bash
npm run migrate
```

### 6. Настройка systemd

```bash
sudo nano /etc/systemd/system/launcher.service
```

```ini
[Unit]
Description=Cheat Launcher Server
After=network.target

[Service]
Type=simple
User=launcher
WorkingDirectory=/home/launcher/launcher-server/backend
ExecStart=/usr/bin/node src/index.js
Restart=on-failure
RestartSec=10
StandardOutput=syslog
StandardError=syslog
SyslogIdentifier=launcher

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable launcher
sudo systemctl start launcher
```

### 7. Настройка Nginx

```bash
sudo nano /etc/nginx/sites-available/launcher
```

```nginx
server {
    listen 80;
    server_name your-domain.com;

    location / {
        proxy_pass http://127.0.0.1:3000;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_cache_bypass $http_upgrade;
    }
}
```

```bash
sudo ln -s /etc/nginx/sites-available/launcher /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl restart nginx
```

### 8. SSL сертификат

```bash
sudo certbot --nginx -d your-domain.com
```

## Загрузка читов на сервер

### Через API (рекомендуется)

```bash
# Логин как админ
TOKEN=$(curl -s -X POST https://your-domain.com/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"your-password"}' | jq -r '.token')

# Загрузка новой версии
curl -X POST https://your-domain.com/api/admin/versions \
  -H "Authorization: Bearer $TOKEN" \
  -F "game_id=cs2" \
  -F "version=1.0.1" \
  -F "changelog=Fixed ESP rendering" \
  -F "file=@./hv_internal.dll"
```

### Вручную

```bash
# Шифрование файла
openssl enc -aes-256-cbc -salt -in cheat.dll -out cheat.dll.enc -pass pass:your-key

# Копирование на сервер
scp cheat.dll.enc user@server:/home/launcher/launcher-server/storage/games/cs2/
```

## Мониторинг

### Просмотр логов

```bash
# Все логи
sudo journalctl -u launcher -f

# Последние ошибки
sudo journalctl -u launcher --since "1 hour ago" -p err
```

### Проверка статуса

```bash
# Статус сервиса
sudo systemctl status launcher

# Проверка API
curl https://your-domain.com/health
```

## Бэкапы

```bash
# Бэкап базы данных
cp /home/launcher/launcher-server/backend/data/launcher.db /backup/launcher-$(date +%Y%m%d).db

# Бэкап файлов
tar -czf /backup/storage-$(date +%Y%m%d).tar.gz /home/launcher/launcher-server/storage/
```

## Безопасность

### Firewall

```bash
sudo ufw allow 22/tcp   # SSH
sudo ufw allow 80/tcp   # HTTP
sudo ufw allow 443/tcp  # HTTPS
sudo ufw enable
```

### Fail2ban

```bash
sudo apt install fail2ban
sudo systemctl enable fail2ban
```

## Масштабирование

Для большого количества пользователей:

1. **PostgreSQL** вместо SQLite
2. **Redis** для сессий и кэширования
3. **CDN** (Cloudflare) для файлов
4. **Load Balancer** для нескольких серверов

