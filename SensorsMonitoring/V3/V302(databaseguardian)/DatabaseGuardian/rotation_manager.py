"""Database rotation and management logic"""
import os
import sqlite3
import jdatetime
from django.conf import settings
from django.db import connections

# Cache for current database name
_current_db_cache = {'name': 'default', 'loaded': False}

def get_current_database():
    """Get current active database name (cached)"""
    global _current_db_cache
    if _current_db_cache['loaded']:
        return _current_db_cache['name']
    return 'default'

def set_current_database(db_name):
    """Set current active database name in cache"""
    global _current_db_cache
    _current_db_cache['name'] = db_name
    _current_db_cache['loaded'] = True

def register_database(db_name, db_path):
    """Register a database in Django settings with all required options"""
    if db_name not in settings.DATABASES:
        settings.DATABASES[db_name] = {
            'ENGINE': 'django.db.backends.sqlite3',
            'NAME': db_path,
            'ATOMIC_REQUESTS': False,
            'AUTOCOMMIT': True,
            'CONN_MAX_AGE': 600,
            'CONN_HEALTH_CHECKS': True,
            'OPTIONS': {'timeout': 30},
            'TIME_ZONE': None,
            'USER': '',
            'PASSWORD': '',
            'HOST': '',
            'PORT': '',
        }

class RotationManager:
    """مدیریت چرخش و آرشیو دیتابیس"""
    
    def __init__(self):
        self.base_dir = settings.BASE_DIR
    
    def get_current_date(self):
        """دریافت تاریخ جاری شمسی"""
        now = jdatetime.datetime.now()
        return now.year, now.month, now.day
    
    def get_current_year(self):
        """دریافت سال جاری شمسی"""
        return jdatetime.datetime.now().year
    
    def get_current_month(self):
        """دریافت ماه جاری شمسی"""
        return jdatetime.datetime.now().month
    
    def get_db_filename(self, year, month, day):
        """ساخت نام فایل دیتابیس با فرمت year-month-day"""
        return f"db_{year}-{month:02d}-{day:02d}.sqlite3"
    
    def get_db_name(self, year, month, day):
        """ساخت نام دیتابیس"""
        return f"db_{year}-{month:02d}-{day:02d}"
    
    def get_db_path(self, filename):
        """دریافت مسیر کامل فایل دیتابیس"""
        return os.path.join(self.base_dir, filename)
    
    def load_all_databases(self):
        """Load all registered databases into Django settings"""
        from .models import DatabaseRegistry
        
        for registry in DatabaseRegistry.objects.all():
            db_path = self.get_db_path(registry.file_path)
            if os.path.exists(db_path):
                register_database(registry.name, db_path)
            
            if registry.is_current:
                set_current_database(registry.name)
    
    def check_rotation_needed(self):
        """بررسی نیاز به چرخش دیتابیس"""
        from .models import DatabaseConfig, DatabaseRegistry
        
        config = DatabaseConfig.get_config()
        current_db = DatabaseRegistry.get_current()
        
        if not current_db:
            return True, "no_current_db"
        
        current_year = self.get_current_year()
        current_month = self.get_current_month()
        
        if config.rotation_mode == 'yearly':
            if current_db.year < current_year:
                return True, "year_changed"
        elif config.rotation_mode == 'monthly':
            if current_db.year < current_year or (current_db.year == current_year and current_db.month < current_month):
                return True, "month_changed"
        
        if config.max_size_mb > 0:
            db_path = self.get_db_path(current_db.file_path)
            if os.path.exists(db_path):
                size_mb = os.path.getsize(db_path) / (1024 * 1024)
                if size_mb >= config.max_size_mb:
                    return True, "size_limit"
        
        return False, None
    
    def create_new_database(self):
        """ایجاد دیتابیس جدید و انتقال داده‌های اصلی"""
        from .models import DatabaseConfig, DatabaseRegistry
        from save_logs.models import Device, Device_Sensor, SensorLogs
        
        config = DatabaseConfig.get_config()
        current_year, current_month, current_day = self.get_current_date()
        
        new_filename = self.get_db_filename(current_year, current_month, current_day)
        new_path = self.get_db_path(new_filename)
        new_db_name = self.get_db_name(current_year, current_month, current_day)
        
        if os.path.exists(new_path):
            return None, "database_exists"
        
        # Get old current database name for copying data
        old_current = DatabaseRegistry.objects.filter(is_current=True).first()
        old_db_name = old_current.name if old_current else 'default'
        
        if old_current:
            old_current.is_current = False
            old_current.status = 'archived'
            old_current.save()
        
        # Create new database file with WAL mode
        conn = sqlite3.connect(new_path)
        conn.execute('PRAGMA journal_mode=WAL;')
        conn.close()
        
        # Register new database in Django
        register_database(new_db_name, new_path)
        
        # Run migrations
        from django.core.management import call_command
        call_command('migrate', database=new_db_name, verbosity=0)
        
        # Copy master data from old database
        self._copy_master_data(old_db_name, new_db_name, config.keep_logs_count)
        
        # Create registry entry
        new_registry = DatabaseRegistry.objects.create(
            name=new_db_name,
            file_path=new_filename,
            year=current_year,
            month=current_month,
            day=current_day,
            status='active',
            is_current=True
        )
        
        # Update cache
        set_current_database(new_db_name)
        
        return new_registry, "success"
    
    def _copy_master_data(self, source_db, target_db, keep_logs_count):
        """کپی داده‌های Device و Device_Sensor به دیتابیس جدید"""
        from save_logs.models import Device, Device_Sensor, SensorLogs
        
        # Copy devices
        devices = Device.objects.using(source_db).all()
        for device in devices:
            device.pk = None
            device.save(using=target_db)
        
        # Copy sensors
        sensors = Device_Sensor.objects.using(source_db).select_related('device').all()
        for sensor in sensors:
            try:
                new_device = Device.objects.using(target_db).get(device_id=sensor.device.device_id)
                old_sensor_type = sensor.sensor_type
                old_device_id = sensor.device.device_id
                
                sensor.pk = None
                sensor.device = new_device
                sensor.save(using=target_db)
                
                # Copy recent logs if configured
                if keep_logs_count > 0:
                    recent_logs = SensorLogs.objects.using(source_db).filter(
                        sensor__device__device_id=old_device_id,
                        sensor__sensor_type=old_sensor_type
                    ).order_by('-CreationDateTime')[:keep_logs_count]
                    
                    new_sensor = Device_Sensor.objects.using(target_db).get(
                        device=new_device, 
                        sensor_type=old_sensor_type
                    )
                    
                    for log in recent_logs:
                        log.pk = None
                        log.sensor = new_sensor
                        log.save(using=target_db)
            except Exception as e:
                print(f"Error copying sensor {sensor}: {e}")
    
    def update_db_stats(self, db_name=None):
        """بروزرسانی آمار دیتابیس"""
        from .models import DatabaseRegistry
        from save_logs.models import SensorLogs
        
        if db_name:
            registries = DatabaseRegistry.objects.filter(name=db_name)
        else:
            registries = DatabaseRegistry.objects.filter(status='active')
        
        for registry in registries:
            try:
                db_path = self.get_db_path(registry.file_path)
                if os.path.exists(db_path):
                    registry.size_mb = os.path.getsize(db_path) / (1024 * 1024)
                
                # Ensure database is registered
                if registry.name in settings.DATABASES:
                    registry.records_count = SensorLogs.objects.using(registry.name).count()
                registry.save()
            except Exception as e:
                print(f"Error updating stats for {registry.name}: {e}")
    
    def scan_existing_databases(self):
        """اسکن و ثبت دیتابیس‌های موجود"""
        from .models import DatabaseRegistry
        
        for filename in os.listdir(self.base_dir):
            if filename.startswith('db_') and filename.endswith('.sqlite3'):
                if not DatabaseRegistry.objects.filter(file_path=filename).exists():
                    # Parse filename: db_1403-09-22.sqlite3 or db_1403_09.sqlite3 or db_1403.sqlite3
                    name_part = filename.replace('db_', '').replace('.sqlite3', '')
                    
                    year, month, day = self.get_current_year(), None, None
                    
                    if '-' in name_part:
                        # New format: 1403-09-22
                        parts = name_part.split('-')
                        year = int(parts[0]) if parts[0].isdigit() else year
                        month = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else None
                        day = int(parts[2]) if len(parts) > 2 and parts[2].isdigit() else None
                    elif '_' in name_part:
                        # Old format: 1403_09
                        parts = name_part.split('_')
                        year = int(parts[0]) if parts[0].isdigit() else year
                        month = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else None
                    elif name_part.isdigit():
                        # Just year: 1403
                        year = int(name_part)
                    
                    db_name = filename.replace('.sqlite3', '')
                    db_path = self.get_db_path(filename)
                    
                    # Register in Django settings
                    register_database(db_name, db_path)
                    
                    DatabaseRegistry.objects.create(
                        name=db_name,
                        file_path=filename,
                        year=year,
                        month=month,
                        day=day,
                        status='archived'
                    )
    
    def initialize(self):
        """راه‌اندازی اولیه سیستم"""
        from .models import DatabaseConfig, DatabaseRegistry
        
        DatabaseConfig.get_config()
        
        self.scan_existing_databases()
        
        if not DatabaseRegistry.objects.filter(is_current=True).exists():
            default_exists = DatabaseRegistry.objects.filter(name='default').first()
            if default_exists:
                default_exists.is_current = True
                default_exists.status = 'active'
                default_exists.save()
            else:
                current_year = self.get_current_year()
                DatabaseRegistry.objects.create(
                    name='default',
                    file_path='db.sqlite3',
                    year=current_year,
                    status='active',
                    is_current=True
                )
        
        # Load all databases into Django settings
        self.load_all_databases()
