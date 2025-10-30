#!/usr/bin/env python3
"""
Simple Web Server Example
A basic HTTP server that can be run as a systemd service.
"""

import http.server
import socketserver
import os
import sys
import signal
import time
from datetime import datetime

# Configuration
PORT = int(os.environ.get('PORT', 8000))
HOST = os.environ.get('HOST', '0.0.0.0')

class MyHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    """Custom HTTP request handler."""
    
    def do_GET(self):
        """Handle GET requests."""
        if self.path == '/':
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            
            html_content = f"""
            <!DOCTYPE html>
            <html>
            <head>
                <title>Simple Web Server</title>
                <style>
                    body {{ font-family: Arial, sans-serif; margin: 40px; }}
                    .container {{ max-width: 600px; margin: 0 auto; }}
                    .status {{ background: #e8f5e8; padding: 20px; border-radius: 5px; }}
                </style>
            </head>
            <body>
                <div class="container">
                    <h1>Simple Web Server</h1>
                    <div class="status">
                        <h2>Server Status</h2>
                        <p><strong>Time:</strong> {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
                        <p><strong>Host:</strong> {HOST}</p>
                        <p><strong>Port:</strong> {PORT}</p>
                        <p><strong>PID:</strong> {os.getpid()}</p>
                        <p><strong>Status:</strong> Running as systemd service</p>
                    </div>
                </div>
            </body>
            </html>
            """
            self.wfile.write(html_content.encode())
        elif self.path == '/health':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            health_data = {
                'status': 'healthy',
                'timestamp': datetime.now().isoformat(),
                'pid': os.getpid(),
                'port': PORT
            }
            import json
            self.wfile.write(json.dumps(health_data).encode())
        else:
            self.send_response(404)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
            self.wfile.write(b'Not Found')
    
    def log_message(self, format, *args):
        """Override to use our custom logging."""
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        print(f"[{timestamp}] {format % args}")

def signal_handler(signum, frame):
    """Handle shutdown signals gracefully."""
    print(f"\n[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Received signal {signum}, shutting down...")
    sys.exit(0)

def main():
    """Main function."""
    # Set up signal handlers
    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)
    
    try:
        with socketserver.TCPServer((HOST, PORT), MyHTTPRequestHandler) as httpd:
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Starting web server...")
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Server running at http://{HOST}:{PORT}")
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] PID: {os.getpid()}")
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Press Ctrl+C to stop")
            
            httpd.serve_forever()
            
    except OSError as e:
        if e.errno == 98:  # Address already in use
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Error: Port {PORT} is already in use")
            sys.exit(1)
        else:
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Error: {e}")
            sys.exit(1)
    except KeyboardInterrupt:
        print(f"\n[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Server stopped by user")
    except Exception as e:
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Unexpected error: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()