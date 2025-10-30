# Django Project Setup for ESP32 QR Code Receiver

## 1. Create Django Project

```bash
# Create a new Django project
django-admin startproject qr_receiver
cd qr_receiver

# Create a new app
python manage.py startapp qr_api
```

## 2. Project Structure

```
qr_receiver/
├── qr_receiver/
│   ├── __init__.py
│   ├── settings.py
│   ├── urls.py
│   └── wsgi.py
├── qr_api/
│   ├── __init__.py
│   ├── models.py
│   ├── views.py
│   ├── urls.py
│   └── apps.py
├── manage.py
└── requirements.txt
```

## 3. Install Dependencies

Create `requirements.txt`:
```
Django>=4.2.0
djangorestframework>=3.14.0
```

Install:
```bash
pip install -r requirements.txt
```

## 4. Configure Settings

Update `qr_receiver/settings.py`:

```python
INSTALLED_APPS = [
    'django.contrib.admin',
    'django.contrib.auth',
    'django.contrib.contenttypes',
    'django.contrib.sessions',
    'django.contrib.messages',
    'django.contrib.staticfiles',
    'rest_framework',  # Add this
    'qr_api',  # Add your app
]

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
```

## 5. Create Models

Update `qr_api/models.py`:

```python
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
```

## 6. Create Views

Update `qr_api/views.py` with the code from `django_qr_receiver.py`:

```python
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.views.decorators.http import require_http_methods
import json
import logging
from datetime import datetime
from .models import QRCodeRecord

logger = logging.getLogger(__name__)

@csrf_exempt
@require_http_methods(["POST"])
def receive_qr_data(request):
    """
    Receive QR code data from ESP32
    """
    try:
        data = json.loads(request.body.decode('utf-8'))
        
        qr_index = data.get('qr_index', 0)
        payload = data.get('payload', '')
        timestamp = data.get('timestamp', 0)
        
        logger.info(f"Received QR code data: index={qr_index}, payload={payload}, timestamp={timestamp}")
        
        # Store in database
        qr_record = QRCodeRecord.objects.create(
            qr_index=qr_index,
            payload=payload,
            timestamp=datetime.fromtimestamp(timestamp / 1000),
            source_ip=request.META.get('REMOTE_ADDR', 'unknown')
        )
        
        # Process different types of QR codes
        if payload.startswith('http'):
            logger.info(f"URL QR code detected: {payload}")
        elif payload.startswith('WIFI:'):
            logger.info(f"WiFi QR code detected: {payload}")
        else:
            logger.info(f"Text QR code detected: {payload}")
        
        return JsonResponse({
            'status': 'success',
            'message': 'QR code data received successfully',
            'record_id': qr_record.id
        }, status=200)
        
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
```

## 7. Configure URLs

Update `qr_api/urls.py`:

```python
from django.urls import path
from . import views

urlpatterns = [
    path('accounts/', views.receive_qr_data, name='receive_qr_data'),
]
```

Update `qr_receiver/urls.py`:

```python
from django.contrib import admin
from django.urls import path, include

urlpatterns = [
    path('admin/', admin.site.urls),
    path('', include('qr_api.urls')),
]
```

## 8. Run Migrations

```bash
python manage.py makemigrations
python manage.py migrate
```

## 9. Start the Server

```bash
python manage.py runserver 192.168.237.15:8001
```

## 10. Test the Endpoint

You can test the endpoint using curl:

```bash
curl -X POST http://192.168.237.15:8001/accounts/ \
  -H "Content-Type: application/json" \
  -d '{"qr_index": 0, "payload": "https://example.com", "timestamp": 1234567890}'
```

## 11. Monitor Logs

Check the logs in `qr_codes.log` or console output to see received QR code data.

## 12. Optional: Admin Interface

Register the model in `qr_api/admin.py`:

```python
from django.contrib import admin
from .models import QRCodeRecord

@admin.register(QRCodeRecord)
class QRCodeRecordAdmin(admin.ModelAdmin):
    list_display = ['qr_index', 'payload', 'timestamp', 'source_ip', 'created_at']
    list_filter = ['created_at', 'source_ip']
    search_fields = ['payload']
    readonly_fields = ['created_at']
```

Create a superuser to access admin:
```bash
python manage.py createsuperuser
```

## 13. Production Considerations

For production deployment:

1. **Security**: Remove `@csrf_exempt` and implement proper authentication
2. **Database**: Use PostgreSQL or MySQL instead of SQLite
3. **Static Files**: Configure static file serving
4. **HTTPS**: Use SSL/TLS certificates
5. **Rate Limiting**: Implement rate limiting to prevent abuse
6. **Monitoring**: Add proper monitoring and alerting

## 14. Alternative: Simple Flask Server

If you prefer a simpler setup, you can use the Flask example included in the `django_qr_receiver.py` file:

```bash
pip install flask
python flask_server.py
```

This will start a simple Flask server on port 8001 that can receive the ESP32 QR code data. 