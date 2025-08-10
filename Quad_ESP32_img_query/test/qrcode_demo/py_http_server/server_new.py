from http.server import BaseHTTPRequestHandler, HTTPServer
import cv2
import numpy as np

class ImageHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path == '/upload':
            print(self)
        else:
            self.send_response(404)
            self.end_headers()

def run(server_class=HTTPServer, handler_class=ImageHandler, port=8000):
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    print(f'Server running on port {port}...')
    httpd.serve_forever()

if __name__ == '__main__':
    run()