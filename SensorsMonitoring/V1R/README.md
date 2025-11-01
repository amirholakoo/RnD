# Sensors Monitoring System V1

A Django-based web application for monitoring and visualizing sensor data from IoT devices in real-time. The system supports multiple devices and sensor types with Persian/Jalali calendar integration.

## Features

- Real-time sensor data collection via REST API
- Multi-device and multi-sensor support
- Interactive dashboard with data visualization
- Persian/Jalali calendar support
- Historical data logging and filtering
- Device management and status tracking
- Prediction data integration

## Project Structure

```
SensorsMonitoring/V1/
├── base/                          # Base app for common functionality
│   ├── migrations/                # Database migrations
│   ├── templatetags/              # Custom template filters
│   │   ├── to_jalali.py          # Jalali date converter
│   │   └── replace.py            # String replacement filter
│   ├── admin.py                   # Admin configuration
│   ├── apps.py                    # App configuration
│   ├── context_processors.py     # Context processors
│   ├── models.py                  # Base models
│   └── views.py                   # Base views
├── config/                        # Django project settings
│   ├── settings.py               # Main settings file
│   ├── urls.py                   # URL routing
│   ├── wsgi.py                   # WSGI configuration
│   └── asgi.py                   # ASGI configuration
├── dashboard/                     # Dashboard app
│   ├── migrations/               # Database migrations
│   ├── admin.py                  # Admin configuration
│   ├── models.py                 # Dashboard models
│   ├── urls.py                   # Dashboard URLs
│   └── views.py                  # Dashboard views & API endpoints
├── save_logs/                     # Data logging app
│   ├── migrations/               # Database migrations
│   ├── middleware.py             # Custom middleware
│   ├── models.py                 # Device, Sensor, and Log models
│   ├── urls.py                   # API URLs
│   └── views.py                  # API endpoints for data collection
├── static/                        # Static files
│   ├── base/                     # Base static files
│   │   ├── css/                  # Stylesheets
│   │   ├── fonts/                # Font files
│   │   ├── img/                  # Images
│   │   └── js/                   # JavaScript files
│   └── dashboard/                # Dashboard static files
├── templates/                     # HTML templates
│   ├── base/                     # Base templates
│   │   ├── base.html            # Base template
│   │   └── header.html          # Header component
│   └── dashboard/                # Dashboard templates
│       ├── dashboard.html       # Main dashboard
│       ├── device_info.html     # Device details
│       └── update_prediction.html # Prediction management
├── manage.py                      # Django management script
├── prediction.json               # Prediction data storage
└── db.sqlite3                    # SQLite database (created after setup)
```

## Setup Instructions

### 1. Clone or Navigate to Project Directory

```bash
cd RnD\SensorsMonitoring\V1
```

### 2. Create Virtual Environment

```bash
python -m venv venv
```

### 3. Activate Virtual Environment

**Windows:**
```bash
venv\Scripts\activate
```

**Linux/Mac:**
```bash
source venv/bin/activate
```

### 4. Install Dependencies

```bash
pip install -r requirements.txt
```

### 5. Apply Database Migrations

```bash
python manage.py makemigrations
```

```bash
python manage.py migrate
```

### 6. Create Superuser (Optional)

```bash
python manage.py createsuperuser
```

### 7. Run Development Server

```bash
python manage.py runserver
```

The application will be available at: `http://127.0.0.1:8000/`

### 8. Access Admin Panel (Optional)

Navigate to: `http://127.0.0.1:8000/admin/`

## API Endpoints

### Data Collection
- `POST /save_logs/post_data/` - Receive sensor data from devices

### Dashboard
- `GET /dashboard/` - Main dashboard view
- `GET /dashboard/device/<device_id>/<sensor_id>/` - Device details
- `POST /dashboard/chart_info_json/<sensor_id>/` - Chart data with filters
- `GET /dashboard/device_info_json/<sensor_id>/` - Sensor data JSON
- `GET /dashboard/sensor_status/` - Real-time sensor status
- `POST /dashboard/add_device/` - Register new device
- `GET/POST /dashboard/update_prediction/` - Manage prediction data

## Configuration

The application is configured to:
- Use SQLite database (default)
- Support Persian locale on Windows and Linux
- Run in DEBUG mode (change in production)
- Accept connections from any host (configure ALLOWED_HOSTS in production)
- Use Asia/Tehran timezone

## Data Format

Devices should send data in the following JSON format:

```json
{
  "device_id": "unique_device_identifier",
  "sensor_type": "temperature",
  "data": {
    "temperature": 25.5,
    "timestamp": 1234567890
  }
}
```

## Notes

- The system automatically creates devices and sensors when they first send data
- Data is retained indefinitely (configure cleanup as needed)
- Static files are served from the `static/` directory
- Templates support Jalali calendar display for timestamps
