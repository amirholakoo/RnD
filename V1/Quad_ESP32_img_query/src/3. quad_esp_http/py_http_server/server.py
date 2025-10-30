from flask import Flask, request
import time

app = Flask(__name__)

@app.route('/test', methods=['GET'])
def test():
    device_id = request.headers.get('X-Device-ID', 'Unknown')
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())
    print(f"Received request from {device_id} at {timestamp}")
    return f"Hello from the server! Device ID: {device_id}", 200

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)