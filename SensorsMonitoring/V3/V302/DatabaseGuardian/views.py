from django.shortcuts import render, redirect
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from .models import DatabaseConfig, DatabaseRegistry, GlobalDatabaseSelection
from .rotation_manager import RotationManager
import json
import os

def settings_view(request):
    """صفحه تنظیمات دیتابیس"""
    config = DatabaseConfig.get_config()
    databases = DatabaseRegistry.objects.all().order_by('-year', '-month')
    current_db = DatabaseRegistry.get_current()
    
    manager = RotationManager()
    manager.update_db_stats()
    
    selection = GlobalDatabaseSelection.get_selection()
    selected_dbs = selection.selected_databases
    
    context = {
        'config': config,
        'databases': databases,
        'current_db': current_db,
        'selected_dbs': selected_dbs,
    }
    return render(request, 'database_guardian/settings.html', context)


@csrf_exempt
def save_config(request):
    """ذخیره تنظیمات"""
    if request.method != 'POST':
        return JsonResponse({'status': 'error', 'message': 'Only POST allowed'}, status=405)
    
    try:
        data = json.loads(request.body)
        config = DatabaseConfig.get_config()
        
        config.rotation_mode = data.get('rotation_mode', config.rotation_mode)
        config.max_size_mb = int(data.get('max_size_mb', config.max_size_mb))
        config.max_records = int(data.get('max_records', config.max_records))
        config.auto_rotate = data.get('auto_rotate', config.auto_rotate)
        config.keep_logs_count = int(data.get('keep_logs_count', config.keep_logs_count))
        config.save()
        
        return JsonResponse({'status': 'ok'})
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)}, status=400)


@csrf_exempt
def save_db_selection(request):
    """ذخیره انتخاب دیتابیس‌ها (مشترک برای همه)"""
    if request.method != 'POST':
        return JsonResponse({'status': 'error', 'message': 'Only POST allowed'}, status=405)
    
    try:
        data = json.loads(request.body)
        selected = data.get('selected_databases', [])
        view_mode = data.get('view_mode', 'current')
        
        selection = GlobalDatabaseSelection.get_selection()
        selection.selected_databases = selected
        selection.view_mode = view_mode
        selection.save()
        
        return JsonResponse({'status': 'ok'})
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)}, status=400)


@csrf_exempt
def rotate_database(request):
    """چرخش دستی دیتابیس"""
    if request.method != 'POST':
        return JsonResponse({'status': 'error', 'message': 'Only POST allowed'}, status=405)
    
    try:
        manager = RotationManager()
        new_db, result = manager.create_new_database()
        
        if result == 'success':
            return JsonResponse({
                'status': 'ok', 
                'message': f'دیتابیس جدید ایجاد شد: {new_db.name}'
            })
        else:
            return JsonResponse({
                'status': 'error', 
                'message': f'خطا در ایجاد دیتابیس: {result}'
            }, status=400)
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)}, status=400)


def check_rotation(request):
    """بررسی نیاز به چرخش"""
    manager = RotationManager()
    needs_rotation, reason = manager.check_rotation_needed()
    
    return JsonResponse({
        'needs_rotation': needs_rotation,
        'reason': reason
    })


def get_db_stats(request):
    """دریافت آمار دیتابیس‌ها"""
    databases = DatabaseRegistry.objects.all().order_by('-year', '-month')
    
    stats = []
    for db in databases:
        stats.append({
            'name': db.name,
            'year': db.year,
            'month': db.month,
            'status': db.status,
            'is_current': db.is_current,
            'size_mb': round(db.size_mb, 2),
            'records_count': db.records_count,
        })
    
    return JsonResponse({'databases': stats})


def initialize_system(request):
    """راه‌اندازی اولیه سیستم"""
    manager = RotationManager()
    manager.initialize()
    
    return JsonResponse({'status': 'ok', 'message': 'سیستم راه‌اندازی شد'})

