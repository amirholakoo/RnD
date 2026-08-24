"""Middleware for multi-database context management

Supports:
1. X-MULTI-DB header (optional override for specific requests)
2. GlobalDatabaseSelection (shared selection for all users)
3. Default: current database fallback
"""
from .managers import set_multi_db_context, clear_multi_db_context


class DatabaseSelectionMiddleware:
    """Set multi-DB context based on global database selection
    
    Priority:
    1. X-MULTI-DB header: optional override
    2. GlobalDatabaseSelection: shared setting for all users
    3. Fallback: current database only
    """
    
    MULTI_DB_HEADER = 'HTTP_X_MULTI_DB'
    
    def __init__(self, get_response):
        self.get_response = get_response
    
    def __call__(self, request):
        self._set_context(request)
        
        try:
            response = self.get_response(request)
        finally:
            clear_multi_db_context()
        
        return response
    
    def _get_header_selection(self, request):
        """Check for X-MULTI-DB header (optional override)"""
        header_value = request.META.get(self.MULTI_DB_HEADER)
        if header_value:
            dbs = [db.strip() for db in header_value.split(',') if db.strip()]
            if dbs:
                return dbs
        return None
    
    def _get_global_selection(self):
        """Get global database selection (shared for all users)"""
        try:
            from .models import GlobalDatabaseSelection
            selection = GlobalDatabaseSelection.get_selection()
            if selection.selected_databases:
                return selection.selected_databases
        except Exception:
            pass
        return None
    
    def _set_context(self, request):
        """Set multi-DB context"""
        try:
            selected_dbs = None
            source = None
            
            # Priority 1: X-MULTI-DB header
            header_dbs = self._get_header_selection(request)
            if header_dbs:
                selected_dbs = header_dbs
                source = 'header'
            
            # Priority 2: Global selection
            if not selected_dbs:
                global_dbs = self._get_global_selection()
                if global_dbs:
                    selected_dbs = global_dbs
                    source = 'global'
            
            if selected_dbs:
                self._apply_selection(selected_dbs, source)
            else:
                self._set_default_context()
                
        except Exception as e:
            print(f"[MultiDB] Middleware error: {e}")
            self._set_default_context()
    
    def _apply_selection(self, selected_dbs, source):
        """Apply database selection"""
        from .rotation_manager import RotationManager
        manager = RotationManager()
        manager.load_all_databases()
        
        num_dbs = len(selected_dbs)
        if num_dbs >= 1:
            set_multi_db_context(selected_dbs, enabled=True)
        else:
            self._set_default_context()
    
    def _set_default_context(self):
        """Set default context (current database only)"""
        try:
            from .models import DatabaseRegistry
            current = DatabaseRegistry.get_current()
            db_name = current.name if current else 'default'
            set_multi_db_context([db_name], enabled=False)
        except Exception:
            set_multi_db_context(['default'], enabled=False)

