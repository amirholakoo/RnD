from django.apps import AppConfig


class SaveLogsConfig(AppConfig):
    default_auto_field = 'django.db.models.BigAutoField'
    name = 'save_logs'

    def ready(self):
        """Initialize WAL mode on startup"""
        from .db_config import enable_wal_mode
        try:
            enable_wal_mode()
        except Exception:
            pass  # Skip if database not ready yet