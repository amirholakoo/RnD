from django.apps import AppConfig


class DatabaseguardianConfig(AppConfig):
    default_auto_field = 'django.db.models.BigAutoField'
    name = 'DatabaseGuardian'
    verbose_name = 'مدیریت دیتابیس'

    def ready(self):
        """Initialize database guardian system on startup"""
        self._inject_smart_managers()
        self._initialize_rotation_system()
    
    def _inject_smart_managers(self):
        """Inject SmartDBManager into models configured in settings.py
        
        User config in settings.py:
            MULTI_DB_MODELS = ['save_logs.Device', 'save_logs.Device_Sensor', 'save_logs.SensorLogs']
            MULTI_DB_BUSINESS_KEYS = {
                'save_logs.Device': ['device_id'],
                'save_logs.Device_Sensor': ['device__device_id', 'sensor_type'],
            }
        """
        try:
            from django.conf import settings
            from django.apps import apps
            from .managers import SmartDBManager, MultiDBQuerySet
            
            model_paths = getattr(settings, 'MULTI_DB_MODELS', [])
            
            for model_path in model_paths:
                try:
                    app_label, model_name = model_path.rsplit('.', 1)
                    model = apps.get_model(app_label, model_name)
                    
                    # Check if already has SmartDBManager
                    if isinstance(model.objects, SmartDBManager):
                        # print(f"[MultiDB] {model_path} already has SmartDBManager")
                        continue
                    
                    # Monkey-patch the existing manager's get_queryset method
                    original_manager = model.objects
                    original_get_queryset = original_manager.get_queryset
                    
                    def make_smart_get_queryset(mgr, orig_qs):
                        def smart_get_queryset():
                            from .managers import get_single_selected_db
                            single_db = get_single_selected_db()
                            if single_db:
                                return MultiDBQuerySet(mgr.model, using=single_db)
                            return MultiDBQuerySet(mgr.model, using=mgr._db)
                        return smart_get_queryset
                    
                    original_manager.get_queryset = make_smart_get_queryset(
                        original_manager, original_get_queryset
                    )
                    
                    # print(f"[MultiDB] Patched manager for {model_path}")
                    
                except Exception as e:
                    pass
                    # print(f"[MultiDB] Failed to patch manager for {model_path}: {e}")
                    
        except Exception as e:
            pass
            # print(f"[MultiDB] SmartDBManager injection error: {e}")
    
    def _initialize_rotation_system(self):
        """Initialize database rotation system"""
        try:
            from django.db import connection
            with connection.cursor() as cursor:
                cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='DatabaseGuardian_databaseregistry';")
                if not cursor.fetchone():
                    return
            
            from .rotation_manager import RotationManager
            manager = RotationManager()
            manager.initialize()
            
            from .models import DatabaseConfig
            config = DatabaseConfig.get_config()
            if config.auto_rotate:
                needs_rotation, reason = manager.check_rotation_needed()
                if needs_rotation:
                    manager.create_new_database()
        except Exception as e:
            pass
            # print(f"DatabaseGuardian init: {e}")
