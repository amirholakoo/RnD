from django.contrib import admin
from .models import DatabaseConfig, DatabaseRegistry, GlobalDatabaseSelection

@admin.register(DatabaseConfig)
class DatabaseConfigAdmin(admin.ModelAdmin):
    list_display = ['rotation_mode', 'max_size_mb', 'auto_rotate']

@admin.register(DatabaseRegistry)
class DatabaseRegistryAdmin(admin.ModelAdmin):
    list_display = ['name', 'year', 'status', 'is_current', 'size_mb']
    list_filter = ['status', 'year', 'is_current']

@admin.register(GlobalDatabaseSelection)
class GlobalDatabaseSelectionAdmin(admin.ModelAdmin):
    list_display = ['view_mode', 'selected_databases']

