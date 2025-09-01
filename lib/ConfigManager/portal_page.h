#ifndef PORTAL_PAGE_H
#define PORTAL_PAGE_H

// 配网门户的HTML页面
auto PORTAL_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 土壤监测系统 - 配网</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .container {
            background: white;
            padding: 30px;
            border-radius: 15px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            width: 100%;
            max-width: 400px;
        }
        .header {
            text-align: center;
            margin-bottom: 30px;
        }
        .header h1 {
            color: #333;
            margin: 0;
            font-size: 24px;
        }
        .header p {
            color: #666;
            margin: 10px 0 0 0;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            color: #333;
            font-weight: bold;
        }
        input[type="text"], input[type="password"], input[type="number"] {
            width: 100%;
            padding: 12px;
            border: 2px solid #ddd;
            border-radius: 8px;
            font-size: 16px;
            box-sizing: border-box;
            transition: border-color 0.3s;
        }
        input[type="text"]:focus, input[type="password"]:focus, input[type="number"]:focus {
            border-color: #667eea;
            outline: none;
        }
        .btn {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 12px 30px;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            cursor: pointer;
            width: 100%;
            transition: transform 0.2s;
        }
        .btn:hover {
            transform: translateY(-2px);
        }
        .btn:active {
            transform: translateY(0);
        }
        .status {
            margin-top: 20px;
            padding: 10px;
            border-radius: 5px;
            text-align: center;
            display: none;
        }
        .status.success {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .status.error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .info {
            background: #d1ecf1;
            color: #0c5460;
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
            border: 1px solid #bee5eb;
        }
        .info h3 {
            margin: 0 0 10px 0;
            font-size: 16px;
        }
        .info ul {
            margin: 0;
            padding-left: 20px;
        }
        .info li {
            margin-bottom: 5px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🌱 ESP32 土壤监测系统</h1>
            <p>请配置网络连接参数</p>
        </div>
        
        <div class="info">
            <h3>📋 配置说明</h3>
            <ul>
                <li>WiFi名称：您的WiFi网络名称</li>
                <li>WiFi密码：您的WiFi网络密码</li>
                <li>服务器IP：数据服务器的IP地址</li>
                <li>服务器端口：数据服务器的端口号</li>
            </ul>
        </div>
        
        <form id="configForm">
            <div class="form-group">
                <label for="ssid">WiFi名称 (SSID)</label>
                <input type="text" id="ssid" name="ssid" placeholder="请输入WiFi名称" required>
            </div>
            
            <div class="form-group">
                <label for="password">WiFi密码</label>
                <input type="password" id="password" name="password" placeholder="请输入WiFi密码" required>
            </div>
            
            <div class="form-group">
                <label for="server_ip">服务器IP地址</label>
                <input type="text" id="server_ip" name="server_ip" placeholder="192.168.1.100" value="192.168.1.100" required>
            </div>
            
            <div class="form-group">
                <label for="server_port">服务器端口</label>
                <input type="number" id="server_port" name="server_port" placeholder="7289" value="7289" min="1" max="65535" required>
            </div>
            
            <div class="form-group">
                <label for="ntp_server">NTP服务器</label>
                <input type="text" id="ntp_server" name="ntp_server" placeholder="ntp1.aliyun.com" value="ntp1.aliyun.com" required>
            </div>
            
            <button type="submit" class="btn">💾 保存配置</button>
        </form>
        
        <div id="status" class="status"></div>
    </div>
    
    <script>
        document.getElementById('configForm').addEventListener('submit', function(e) {
            e.preventDefault();
            
            const formData = new FormData(this);
            const data = {
                ssid: formData.get('ssid'),
                password: formData.get('password'),
                server_ip: formData.get('server_ip'),
                server_port: formData.get('server_port'),
                ntp_server: formData.get('ntp_server')
            };
            
            // 显示加载状态
            const btn = document.querySelector('.btn');
            const originalText = btn.textContent;
            btn.textContent = '⏳ 保存中...';
            btn.disabled = true;
            
            // 发送配置到ESP32
            fetch('/save', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(data)
            })
            .then(response => response.json())
            .then(result => {
                const status = document.getElementById('status');
                if (result.success) {
                    status.className = 'status success';
                    status.textContent = '✅ 配置保存成功！设备将重启并连接到指定网络。';
                    status.style.display = 'block';
                    
                    // 3秒后隐藏状态
                    setTimeout(() => {
                        status.style.display = 'none';
                    }, 3000);
                } else {
                    status.className = 'status error';
                    status.textContent = '❌ 配置保存失败：' + (result.message || '未知错误');
                    status.style.display = 'block';
                }
            })
            .catch(error => {
                const status = document.getElementById('status');
                status.className = 'status error';
                status.textContent = '❌ 网络错误：' + error.message;
                status.style.display = 'block';
            })
            .finally(() => {
                // 恢复按钮状态
                btn.textContent = originalText;
                btn.disabled = false;
            });
        });
        
        // 页面加载完成后的提示
        window.addEventListener('load', function() {
            console.log('ESP32 土壤监测系统配网页面已加载');
        });
    </script>
</body>
</html>
)rawliteral";

#endif // PORTAL_PAGE_H 