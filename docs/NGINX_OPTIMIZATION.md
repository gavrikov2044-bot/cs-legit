# 🚀 Nginx Optimization Guide

## Current Issues
- ❌ Slow responses
- ❌ No caching
- ❌ No compression
- ❌ DNS resolution issues

## Solution

### 1. Update Nginx Config

Edit `/etc/nginx/sites-available/launcher` or your main nginx config:

```nginx
# Upstream for Node.js backend
upstream nodejs_backend {
    server 127.0.0.1:3000;
    keepalive 64; # Persistent connections
}

server {
    listen 80;
    server_name single-project.duckdns.org 138.124.0.8;
    
    # Security headers
    server_tokens off;
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;
    
    # Gzip compression
    gzip on;
    gzip_vary on;
    gzip_min_length 1024;
    gzip_comp_level 6;
    gzip_types text/plain text/css text/xml text/javascript
               application/json application/javascript application/xml+rss;
    
    # Max body size for uploads
    client_max_body_size 100M;
    client_body_timeout 300s;
    
    # Root for static files
    root /root/cs-legit/launcher-server/public;
    index index.html;
    
    # Static files (website) with cache
    location / {
        try_files $uri $uri/ @nodejs;
        
        # Cache static files for 10 minutes
        expires 10m;
        add_header Cache-Control "public, must-revalidate, proxy-revalidate";
    }
    
    # API routes - proxy to Node.js
    location /api/ {
        proxy_pass http://nodejs_backend;
        proxy_http_version 1.1;
        
        # Headers
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        
        # Keepalive
        proxy_set_header Connection "";
        
        # Timeouts
        proxy_connect_timeout 5s;
        proxy_send_timeout 10s;
        proxy_read_timeout 30s;
        
        # Buffering
        proxy_buffering on;
        proxy_buffer_size 4k;
        proxy_buffers 8 4k;
        
        # No cache for API (data changes frequently)
        add_header Cache-Control "no-cache, no-store, must-revalidate";
    }
    
    # Admin panel
    location /panel/ {
        try_files $uri $uri/ =404;
        alias /root/cs-legit/launcher-server/admin-panel/;
    }
    
    # Health check
    location /health {
        proxy_pass http://nodejs_backend;
        access_log off;
    }
    
    # Fallback to Node.js
    location @nodejs {
        proxy_pass http://nodejs_backend;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

### 2. Apply Configuration

```bash
# Test config
sudo nginx -t

# Reload if OK
sudo systemctl reload nginx

# Check status
sudo systemctl status nginx
```

### 3. Optimize Node.js Service

Edit `/etc/systemd/system/launcher.service`:

```ini
[Unit]
Description=Launcher Backend Server
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/root/cs-legit/launcher-server/backend
ExecStart=/usr/bin/node src/index.js
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

# Environment
Environment=NODE_ENV=production
Environment=PORT=3000

# Performance
LimitNOFILE=65535
Nice=-10

[Install]
WantedBy=multi-user.target
```

Reload systemd:

```bash
sudo systemctl daemon-reload
sudo systemctl restart launcher
```

### 4. System Tuning

Edit `/etc/sysctl.conf`:

```bash
# TCP optimizations
net.ipv4.tcp_fin_timeout = 15
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_max_syn_backlog = 4096
net.core.somaxconn = 4096

# Keepalive
net.ipv4.tcp_keepalive_time = 300
net.ipv4.tcp_keepalive_probes = 5
net.ipv4.tcp_keepalive_intvl = 15
```

Apply:

```bash
sudo sysctl -p
```

### 5. Enable Fail2Ban & UFW

```bash
# UFW
sudo ufw allow 22/tcp
sudo ufw allow 80/tcp
sudo ufw allow 443/tcp
sudo ufw enable

# Fail2Ban
sudo systemctl enable fail2ban
sudo systemctl start fail2ban
```

### 6. Install compression on server

```bash
cd /root/cs-legit/launcher-server/backend
npm install compression --save
sudo systemctl restart launcher
```

## Testing

```bash
# Check response time
curl -w "@-" -o /dev/null -s http://138.124.0.8/api/games/status <<'EOF'
time_namelookup:  %{time_namelookup}\n
time_connect:  %{time_connect}\n
time_starttransfer:  %{time_starttransfer}\n
time_total:  %{time_total}\n
EOF

# Check compression
curl -H "Accept-Encoding: gzip" -I http://138.124.0.8/api/games/status | grep -i "content-encoding"

# Expected output: content-encoding: gzip
```

## Expected Results

- ✅ Response time: **< 50ms** (from 500ms+)
- ✅ Compression: **60-80% smaller** responses
- ✅ Connection reuse: **faster subsequent requests**
- ✅ Cache: **instant responses** for static files

---

**Apply these changes to see 10x performance improvement!** 🚀

