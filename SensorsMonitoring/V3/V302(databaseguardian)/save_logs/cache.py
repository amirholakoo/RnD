"""In-memory cache for device and sensor lookups - database aware"""
from threading import Lock

class DeviceCache:
    """Thread-safe cache for device and sensor objects - validates database"""
    
    def __init__(self):
        self._devices = {}  # {(device_id, db_name): device_obj}
        self._sensors = {}  # {(device_id, sensor_type, db_name): sensor_obj}
        self._lock = Lock()
    
    def _get_current_db(self):
        """Get current database name"""
        try:
            from DatabaseGuardian.managers import get_current_write_db
            return get_current_write_db()
        except Exception:
            return 'default'
    
    def get_device(self, device_id):
        """Get device only if it's from current database"""
        with self._lock:
            current_db = self._get_current_db()
            device = self._devices.get((device_id, current_db))
            if device:
                # Verify it's still from current DB
                if getattr(device, '_state', None) and device._state.db == current_db:
                    return device
                # Stale cache - remove it
                del self._devices[(device_id, current_db)]
            return None
    
    def set_device(self, device_id, device_obj):
        """Cache device with its database"""
        with self._lock:
            db_name = getattr(device_obj._state, 'db', None) or self._get_current_db()
            self._devices[(device_id, db_name)] = device_obj
    
    def get_sensor(self, device_id, sensor_type):
        """Get sensor only if it's from current database"""
        with self._lock:
            current_db = self._get_current_db()
            sensor = self._sensors.get((device_id, sensor_type, current_db))
            if sensor:
                # Verify it's still from current DB
                if getattr(sensor, '_state', None) and sensor._state.db == current_db:
                    return sensor
                # Stale cache - remove it
                del self._sensors[(device_id, sensor_type, current_db)]
            return None
    
    def set_sensor(self, device_id, sensor_type, sensor_obj):
        """Cache sensor with its database"""
        with self._lock:
            db_name = getattr(sensor_obj._state, 'db', None) or self._get_current_db()
            self._sensors[(device_id, sensor_type, db_name)] = sensor_obj
    
    def clear(self):
        """Clear all cached objects"""
        with self._lock:
            self._devices.clear()
            self._sensors.clear()

# Global cache instance
device_cache = DeviceCache()

