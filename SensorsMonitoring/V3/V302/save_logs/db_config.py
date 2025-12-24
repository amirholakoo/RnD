"""Database configuration for optimal SQLite performance"""
from django.db import connection

def enable_wal_mode():
    """Enable WAL mode for better concurrent access and crash recovery"""
    with connection.cursor() as cursor:
        cursor.execute('PRAGMA journal_mode=WAL;')
        cursor.execute('PRAGMA synchronous=NORMAL;')
        cursor.execute('PRAGMA cache_size=-64000;')  # 64MB cache
        cursor.execute('PRAGMA temp_store=MEMORY;')
        cursor.execute('PRAGMA mmap_size=268435456;')  # 256MB mmap

