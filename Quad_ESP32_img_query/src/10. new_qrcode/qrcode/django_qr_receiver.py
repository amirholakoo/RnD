# Django QR Code Receiver Example
# This example shows how to receive QR code data from ESP32

from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.views.decorators.http import require_http_methods
import json
import logging
from datetime import datetime

# Set up logging
logger = logging.getLogger(__name__)

@csrf_exempt  # Disable CSRF for ESP32 requests
@require_http_methods(["POST"])
def receive_qr_data(request):
    """
    Receive QR code data from ESP32
    Expected JSON format:
    {
        "qr_index": 0,
        "payload": "QR_CODE_CONTENT",
        "timestamp": 1234567890
    }
    """
    try:
        # Parse JSON data from request body
        data = json.loads(request.body.decode('utf-8'))
        
        # Extract fields from the JSON
        qr_index = data.get('qr_index', 0)
        payload = data.get('payload', '')
        timestamp = data.get('timestamp', 0)
        
        # Log the received data
        logger.info(f"Received QR code data: index={qr_index}, payload={payload}, timestamp={timestamp}")
        
        # Convert timestamp to readable format
        timestamp_readable = datetime.fromtimestamp(timestamp / 1000).strftime('%Y-%m-%d %H:%M:%S')
        
        # Here you can add your business logic to process the QR code data
        # For example:
        # - Store in database
        # - Validate QR code content
        # - Trigger other actions based on QR content
        
        # Example: Store in database (uncomment if you have a model)
        # qr_record = QRCodeRecord.objects.create(
        #     qr_index=qr_index,
        #     payload=payload,
        #     timestamp=datetime.fromtimestamp(timestamp / 1000),
        #     source_ip=request.META.get('REMOTE_ADDR', 'unknown')
        # )
        
        # Example: Process different types of QR codes
        if payload.startswith('http'):
            logger.info(f"URL QR code detected: {payload}")
            # Handle URL QR codes
        elif payload.startswith('WIFI:'):
            logger.info(f"WiFi QR code detected: {payload}")
            # Handle WiFi QR codes
        else:
            logger.info(f"Text QR code detected: {payload}")
            # Handle text QR codes
        
        # Return success response
        response_data = {
            'status': 'success',
            'message': 'QR code data received successfully',
            'received_data': {
                'qr_index': qr_index,
                'payload': payload,
                'timestamp': timestamp,
                'timestamp_readable': timestamp_readable,
                'source_ip': request.META.get('REMOTE_ADDR', 'unknown')
            }
        }
        
        return JsonResponse(response_data, status=200)
        
    except json.JSONDecodeError as e:
        logger.error(f"Invalid JSON received: {e}")
        return JsonResponse({
            'status': 'error',
            'message': 'Invalid JSON format'
        }, status=400)
        
    except Exception as e:
        logger.error(f"Error processing QR code data: {e}")
        return JsonResponse({
            'status': 'error',
            'message': 'Internal server error'
        }, status=500)


# Optional: Django model for storing QR code data
"""
from django.db import models

class QRCodeRecord(models.Model):
    qr_index = models.IntegerField()
    payload = models.TextField()
    timestamp = models.DateTimeField()
    source_ip = models.GenericIPAddressField()
    created_at = models.DateTimeField(auto_now_add=True)
    
    class Meta:
        ordering = ['-created_at']
    
    def __str__(self):
        return f"QR[{self.qr_index}]: {self.payload[:50]}..."
"""

# URL configuration (urls.py)
"""
from django.urls import path
from . import views

urlpatterns = [
    path('accounts/', views.receive_qr_data, name='receive_qr_data'),
]
"""

# Settings configuration (settings.py additions)
"""
# Add to your Django settings.py

# Allow ESP32 IP address
ALLOWED_HOSTS = ['192.168.237.15', 'localhost', '127.0.0.1']

# Configure logging
LOGGING = {
    'version': 1,
    'disable_existing_loggers': False,
    'handlers': {
        'file': {
            'level': 'INFO',
            'class': 'logging.FileHandler',
            'filename': 'qr_codes.log',
        },
        'console': {
            'level': 'INFO',
            'class': 'logging.StreamHandler',
        },
    },
    'loggers': {
        'django': {
            'handlers': ['console', 'file'],
            'level': 'INFO',
            'propagate': True,
        },
    },
}
"""

# Example usage with Django REST Framework (alternative approach)
"""
from rest_framework.decorators import api_view
from rest_framework.response import Response
from rest_framework import status

@api_view(['POST'])
def receive_qr_data_drf(request):
    '''
    Django REST Framework version for receiving QR code data
    '''
    try:
        qr_index = request.data.get('qr_index', 0)
        payload = request.data.get('payload', '')
        timestamp = request.data.get('timestamp', 0)
        
        # Process the data here
        
        return Response({
            'status': 'success',
            'message': 'QR code data received',
            'data': {
                'qr_index': qr_index,
                'payload': payload,
                'timestamp': timestamp
            }
        }, status=status.HTTP_200_OK)
        
    except Exception as e:
        return Response({
            'status': 'error',
            'message': str(e)
        }, status=status.HTTP_500_INTERNAL_SERVER_ERROR)
"""

# Example: Simple Flask alternative (if you prefer Flask)
"""
from flask import Flask, request, jsonify
import json
from datetime import datetime

app = Flask(__name__)

@app.route('/accounts/', methods=['POST'])
def receive_qr_data():
    try:
        data = request.get_json()
        
        qr_index = data.get('qr_index', 0)
        payload = data.get('payload', '')
        timestamp = data.get('timestamp', 0)
        
        print(f"Received QR code {qr_index}: {payload}")
        
        return jsonify({
            'status': 'success',
            'message': 'QR code data received successfully'
        }), 200
        
    except Exception as e:
        return jsonify({
            'status': 'error',
            'message': str(e)
        }), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8001, debug=True)
""" 