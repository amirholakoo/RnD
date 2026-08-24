"""Database router for multi-database support"""

class SensorDataRouter:
    """روتر برای هدایت کوئری‌های save_logs به دیتابیس مناسب"""
    
    # All save_logs models that should use the current rotated database
    sensor_models = {'device', 'device_sensor', 'sensorlogs'}
    guardian_models = {'databaseconfig', 'databaseregistry', 'globaldatabaseselection'}
    
    def db_for_read(self, model, **hints):
        """تعیین دیتابیس برای خواندن
        
        CRITICAL: In multi-DB mode, return None to let QuerySet._db be respected.
        This allows our MultiDBQuerySet to query each selected database.
        """
        # Guardian models always use default
        if model._meta.model_name in self.guardian_models:
            return 'default'
        
        # Check if multi-DB mode is active
        if self._is_multi_db_mode():
            # Return None so Django respects the QuerySet's _db setting
            # This is ESSENTIAL for multi-DB reads to work
            return None
        
        # Respect the instance's database for FK lookups (e.g., sensor.device)
        instance = hints.get('instance')
        if instance is not None:
            return instance._state.db
        
        # Single-DB mode: use current database
        if model._meta.model_name in self.sensor_models:
            return self._get_current_db()
        return 'default'

    def db_for_write(self, model, **hints):
        """تعیین دیتابیس برای نوشتن - ALWAYS routes to current DB"""
        if model._meta.model_name in self.guardian_models:
            return 'default'
        if model._meta.model_name in self.sensor_models:
            return self._get_current_db()
        return 'default'
    
    def _is_multi_db_mode(self):
        """Check if multi-DB mode is active (thread-local context)"""
        try:
            from .managers import is_multi_db_mode
            return is_multi_db_mode()
        except Exception:
            return False

    def allow_relation(self, obj1, obj2, **hints):
        """اجازه رابطه بین مدل‌ها"""
        return True

    def allow_migrate(self, db, app_label, model_name=None, **hints):
        """اجازه مایگریشن"""
        if app_label == 'DatabaseGuardian':
            return db == 'default'
        return True

    def _get_current_db(self):
        """دریافت دیتابیس فعال از کش"""
        try:
            from .rotation_manager import get_current_database
            return get_current_database()
        except:
            return 'default'


def get_selected_databases():
    """دریافت دیتابیس‌های انتخاب شده (مشترک برای همه)"""
    try:
        from .models import GlobalDatabaseSelection, DatabaseRegistry
        selection = GlobalDatabaseSelection.get_selection()
        if selection and selection.selected_databases:
            return selection.selected_databases
        current = DatabaseRegistry.get_current()
        return [current.name] if current else ['default']
    except:
        return ['default']
