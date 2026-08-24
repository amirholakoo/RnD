"""Smart Database Manager for multi-database queries
Configuration via settings.py - no changes needed in user models/views
"""
from django.db import models
from django.conf import settings
from threading import local

_thread_locals = local()

# Cache for business keys config
_business_keys_cache = None


def _get_business_keys_config():
    """Get business keys configuration from settings"""
    global _business_keys_cache
    if _business_keys_cache is None:
        _business_keys_cache = getattr(settings, 'MULTI_DB_BUSINESS_KEYS', {})
    return _business_keys_cache


def set_multi_db_context(selected_dbs, enabled=True):
    """Set multi-DB context in thread-local storage"""
    _thread_locals.multi_db_enabled = enabled
    _thread_locals.selected_dbs = selected_dbs


def get_multi_db_context():
    """Get current multi-DB context"""
    enabled = getattr(_thread_locals, 'multi_db_enabled', False)
    selected_dbs = getattr(_thread_locals, 'selected_dbs', [])
    return enabled, selected_dbs


def is_multi_db_mode():
    """Check if multi-DB mode is active (more than 1 DB selected)"""
    enabled, selected_dbs = get_multi_db_context()
    return enabled and len(selected_dbs) > 1


def get_single_selected_db():
    """Get single selected DB name if only one is selected"""
    enabled, selected_dbs = get_multi_db_context()
    if enabled and len(selected_dbs) == 1:
        return selected_dbs[0]
    return None


def clear_multi_db_context():
    """Clear multi-DB context"""
    _thread_locals.multi_db_enabled = False
    _thread_locals.selected_dbs = []


def get_current_write_db():
    """Get the current database for write operations"""
    try:
        from .rotation_manager import get_current_database
        db_name = get_current_database()
        if db_name == 'default':
            try:
                from .models import DatabaseRegistry
                current = DatabaseRegistry.objects.filter(is_current=True).first()
                if current:
                    return current.name
            except Exception:
                pass
        return db_name
    except Exception:
        return 'default'


def is_safe_for_write(obj):
    """Check if an object is safe to use as FK for write operations
    
    Returns True if object is from current database, False otherwise.
    Use this before using an object as FK in create/update operations.
    """
    if obj is None:
        return False
    current_db = get_current_write_db()
    obj_db = getattr(obj._state, 'db', None)
    return obj_db == current_db or obj_db is None


def ensure_write_safe(obj, model_name="Object"):
    """Validate object is from current database, raise error if not
    
    Call this before using an object as FK in write operations.
    """
    if not is_safe_for_write(obj):
        current_db = get_current_write_db()
        obj_db = getattr(obj._state, 'db', None) if obj else None
        raise ValueError(
            f"{model_name} from database '{obj_db}' cannot be used as FK "
            f"for writes to '{current_db}'. Fetch from current DB first."
        )


class MultiDBQuerySet(models.QuerySet):
    """QuerySet that aggregates results from multiple databases"""
    
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._smart_db_mode = True
        self._multi_db_slice = None  # Store slice for post-aggregation
    
    def _clone(self):
        """Preserve flags when cloning"""
        clone = super()._clone()
        clone._smart_db_mode = getattr(self, '_smart_db_mode', True)
        clone._multi_db_slice = getattr(self, '_multi_db_slice', None)
        return clone
    
    def using(self, alias):
        """Override using() to disable smart mode when explicitly set"""
        clone = super().using(alias)
        clone._smart_db_mode = False
        return clone
    
    def __getitem__(self, k):
        """Override slicing - apply after multi-DB aggregation"""
        if self._should_use_multi_db():
            if isinstance(k, slice):
                # Store slice for post-aggregation, don't apply to each DB
                clone = self._clone()
                clone._multi_db_slice = k
                # Clear Django's internal slice to fetch all from each DB
                clone.query.low_mark = 0
                clone.query.high_mark = None
                return clone
            elif isinstance(k, int):
                # Single item access - aggregate from all DBs then index
                if k < 0:
                    raise ValueError("Negative indexing not supported in multi-DB mode")
                results = list(self)
                if k < len(results):
                    return results[k]
                raise IndexError("list index out of range")
        return super().__getitem__(k)
    
    def _should_use_multi_db(self):
        """Check if multi-DB mode should be used for this queryset"""
        # Only use multi-DB if: smart mode enabled AND thread-local enabled AND >1 DB selected
        return getattr(self, '_smart_db_mode', True) and is_multi_db_mode()
    
    def _get_model_key(self, model):
        """Get model key for config lookup (app_label.model_name)"""
        return f"{model._meta.app_label}.{model._meta.object_name}"
    
    def _get_dedup_field(self):
        """Get deduplication field from config or auto-detect"""
        config = _get_business_keys_config()
        model_key = self._get_model_key(self.model)
        
        # Check config first
        if model_key in config:
            fields = config[model_key]
            if isinstance(fields, (list, tuple)) and len(fields) > 1:
                return tuple(fields)
            elif isinstance(fields, (list, tuple)) and len(fields) == 1:
                return fields[0]
            return fields
        
        # Auto-detect: find unique field (excluding 'id')
        for field in self.model._meta.fields:
            if field.unique and field.name != 'id':
                return field.name
        
        return None  # No dedup - include all records
    
    def _get_business_key_for_model(self, model):
        """Get business key fields for a model from config or auto-detect"""
        config = _get_business_keys_config()
        model_key = self._get_model_key(model)
        
        if model_key in config:
            return config[model_key]
        
        # Auto-detect: find unique field
        for field in model._meta.fields:
            if field.unique and field.name != 'id':
                return [field.name]
        
        return None
    
    def _convert_fk_to_business_key(self, kwargs):
        """Convert FK model instances to business key lookups for cross-DB compatibility"""
        from django.db import models as django_models
        
        converted = {}
        for key, value in kwargs.items():
            if isinstance(value, django_models.Model):
                key_prefix = key
                
                try:
                    business_keys = self._get_business_key_for_model(value.__class__)
                    
                    if business_keys:
                        for bk in business_keys:
                            # Navigate through FK chain if needed (e.g., 'device__device_id')
                            parts = bk.split('__')
                            attr_value = value
                            for part in parts:
                                attr_value = getattr(attr_value, part)
                            new_key = f"{key_prefix}__{bk}"
                            converted[new_key] = attr_value
                    else:
                        # No business key found, keep original (will use PK)
                        converted[key] = value
                except Exception:
                    converted[key] = value
            else:
                converted[key] = value
        
        return converted
    
    def filter(self, *args, **kwargs):
        """Override filter to convert FK instances to business keys in multi-DB mode"""
        if self._should_use_multi_db() and kwargs:
            kwargs = self._convert_fk_to_business_key(kwargs)
        return super().filter(*args, **kwargs)
    
    def exclude(self, *args, **kwargs):
        """Override exclude to convert FK instances to business keys in multi-DB mode"""
        if self._should_use_multi_db() and kwargs:
            kwargs = self._convert_fk_to_business_key(kwargs)
        return super().exclude(*args, **kwargs)
    
    def get(self, *args, **kwargs):
        """Override get - search ALL databases, prioritize current DB
        
        .get() is typically used for reading/displaying data (e.g., by URL id).
        We search all databases, starting with current DB.
        WARNING: Don't use .get() result as FK for writes without validation!
        """
        if self._should_use_multi_db():
            current_db = self._get_current_db()
            _, selected_dbs = get_multi_db_context()
            
            # Convert FK instances to business keys
            if kwargs:
                kwargs = self._convert_fk_to_business_key(kwargs)
            
            # Try current DB first
            if current_db in selected_dbs:
                try:
                    return self._clone_for_db(current_db).get(*args, **kwargs)
                except self.model.DoesNotExist:
                    pass
            
            # Then try other DBs
            for db_name in selected_dbs:
                if db_name != current_db:
                    try:
                        return self._clone_for_db(db_name).get(*args, **kwargs)
                    except self.model.DoesNotExist:
                        continue
            
            raise self.model.DoesNotExist(
                f"{self.model._meta.object_name} matching query does not exist."
            )
        return super().get(*args, **kwargs)
    
    def _get_dedup_key(self, obj, dedup_field):
        """Extract deduplication key from object"""
        try:
            if isinstance(dedup_field, tuple):
                key_parts = []
                for field in dedup_field:
                    value = obj
                    for part in field.split('__'):
                        value = getattr(value, part)
                    key_parts.append(value)
                return tuple(key_parts)
            return getattr(obj, dedup_field)
        except Exception:
            # FK might be broken (orphan record), return unique key to include it
            return (id(obj),)
    
    def _aggregate_results(self, queryset_func):
        """Aggregate results from multiple databases with deduplication"""
        _, selected_dbs = get_multi_db_context()
        current_db = self._get_current_db()
        dedup_field = self._get_dedup_field()
        
        results = []
        seen = set()
        db_counts = {}
        
        # Log which DBs we're querying and which is current
        print(f"[MultiDB] Aggregating {self.model._meta.model_name} from {selected_dbs}, current={current_db}")
        
        for db_name in selected_dbs:
            db_count = 0
            try:
                qs = queryset_func(db_name)
                for obj in qs:
                    try:
                        if dedup_field is None:
                            obj._source_db = db_name
                            results.append(obj)
                            db_count += 1
                        else:
                            key = self._get_dedup_key(obj, dedup_field)
                            if key not in seen:
                                seen.add(key)
                                obj._source_db = db_name
                                results.append(obj)
                                db_count += 1
                    except Exception as e:
                        print(f"[MultiDB] Skip object in {db_name}: {e}")
                db_counts[db_name] = db_count
            except Exception as e:
                print(f"[MultiDB] Error querying {db_name}: {e}")
                db_counts[db_name] = f"ERROR: {e}"
        
        print(f"[MultiDB] {self.model._meta.model_name} results: {db_counts}, total: {len(results)}")
        
        # Sort by CreationDateTime ascending (oldest first) for chart compatibility
        if self.model._meta.model_name == 'sensorlogs' and results:
            results.sort(key=lambda x: x.CreationDateTime or 0, reverse=False)
        
        return results
    
    def _clone_for_db(self, db_name):
        """Create a clone of this queryset for specific database"""
        clone = super()._clone()
        clone._db = db_name
        clone._smart_db_mode = False  # Disable multi-DB for this clone
        return clone
    
    def iterator(self, chunk_size=None):
        """Override iterator for multi-DB mode"""
        if self._should_use_multi_db():
            results = self._aggregate_results(
                lambda db: self._clone_for_db(db).iterator(chunk_size)
            )
            yield from results
        else:
            yield from super().iterator(chunk_size)
    
    def __iter__(self):
        """Override iteration for multi-DB mode"""
        if self._should_use_multi_db():
            _, selected_dbs = get_multi_db_context()
            print(f"[MultiDB] {self.model._meta.model_name}.__iter__ using multi-DB: {selected_dbs}")
            
            def get_qs_for_db(db_name):
                # Clone without slice - fetch all from each DB
                clone = self._clone_for_db(db_name)
                clone.query.low_mark = 0
                clone.query.high_mark = None
                return super(MultiDBQuerySet, clone).__iter__()
            
            results = self._aggregate_results(get_qs_for_db)
            
            # Apply stored slice after aggregation
            stored_slice = getattr(self, '_multi_db_slice', None)
            if stored_slice:
                results = results[stored_slice]
                print(f"[MultiDB] Applied slice {stored_slice.start}:{stored_slice.stop}, got {len(results)} items")
            
            return iter(results)
        return super().__iter__()
    
    def count(self):
        """Override count for multi-DB mode"""
        if self._should_use_multi_db():
            _, selected_dbs = get_multi_db_context()
            total = 0
            for db_name in selected_dbs:
                try:
                    total += self._clone_for_db(db_name).count()
                except Exception:
                    pass
            return total
        return super().count()
    
    def exists(self):
        """Override exists for multi-DB mode"""
        if self._should_use_multi_db():
            _, selected_dbs = get_multi_db_context()
            for db_name in selected_dbs:
                try:
                    if self._clone_for_db(db_name).exists():
                        return True
                except Exception:
                    pass
            return False
        return super().exists()
    
    def first(self):
        """Override first - ONLY check current DB for write safety
        
        .first() is typically used before writes (e.g., "if not exists, create").
        Always use current DB to ensure FK consistency.
        For multi-DB reads, use iteration or .all() instead.
        """
        if self._should_use_multi_db():
            # CRITICAL: Only check current database to avoid FK issues
            current_db = self._get_current_db()
            try:
                return self._clone_for_db(current_db).first()
            except Exception as e:
                print(f"[MultiDB] first() error on {current_db}: {e}")
                return None
        return super().first()
    
    def _get_current_db(self):
        """Get current database for writes"""
        return get_current_write_db()
    
    def last(self):
        """Override last - search ALL databases for reading
        
        Returns last item from aggregated results across all selected databases.
        """
        if self._should_use_multi_db():
            results = list(self)
            return results[-1] if results else None
        return super().last()


class SmartDBManager(models.Manager):
    """Manager that automatically switches between single/multi DB mode"""
    
    def get_queryset(self):
        """Return appropriate queryset based on context"""
        # If single DB is explicitly selected, use it
        single_db = get_single_selected_db()
        if single_db:
            return MultiDBQuerySet(self.model, using=single_db)
        return MultiDBQuerySet(self.model, using=self._db)

