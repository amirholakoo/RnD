from django.shortcuts import render, redirect, get_object_or_404
from .models import Warehouse, WarehouseVision
from django.contrib import messages
from Visions.models import Vision

def warehouses(request):
    warehouses = Warehouse.objects.all()
    context = {
        'warehouses': warehouses
    }
    return render(request, 'warehouse/warehouse.html', context)


def add(request):
    if request.method == 'POST':
        name = request.POST.get('name')
        description = request.POST.get('description')
        location = request.POST.get('location')
        vision = request.POST.getlist('vision')
        if vision:
            vision = list(dict.fromkeys(vision))
        print(vision,"______________________")
        try:
            warehouse = Warehouse(name=name, description=description, location=location)
            warehouse.save()
            for v in vision:
                item = WarehouseVision(warehouse=warehouse, vision=Vision.objects.get(id=int(v)))
                item.save()

            messages.success(request, 'انبار با موفقیت افزوده شد')
            return redirect('warehouses')
        except Exception as e:
            print(e)
            messages.error(request, 'خطا در افزودن انبار')
    context = {
        'visions': Vision.objects.all()
    }
    return render(request, 'warehouse/add.html', context)

def edit(request, warehouse_id):
    warehouse = get_object_or_404(Warehouse, id=warehouse_id)
    if request.method == 'POST':
        name = request.POST.get('name')
        description = request.POST.get('description')
        location = request.POST.get('location')
        warehouse.name = name
        warehouse.description = description
        warehouse.location = location
        vision = request.POST.getlist('vision')
        if vision:
            vision = list(dict.fromkeys(vision))
        print(vision,"______________________")
        try:
            WarehouseVision.objects.filter(warehouse=warehouse).delete()
            for v in vision:
                item = WarehouseVision(warehouse=warehouse, vision=Vision.objects.get(id=int(v)))
                item.save()
            warehouse.save()
            messages.success(request, 'انبار با موفقیت ویرایش شد')
        except Exception as e:
            print(e)
            messages.error(request, 'خطا در ویرایش انبار')
    context = {
        'warehouse': warehouse,
        'visions': WarehouseVision.objects.filter(warehouse=warehouse),
        'all_visions': Vision.objects.all()
    }
    return render(request, 'warehouse/edit.html', context)

def delete(request, warehouse_id):
    warehouse = get_object_or_404(Warehouse, id=warehouse_id)
    try:
        warehouse.Is_Deleted = True
        warehouse.save()
        messages.success(request, 'انبار با موفقیت حذف شد')
    except Exception as e:
        messages.error(request, 'خطا در حذف انبار')
    return redirect('warehouses')

def view(request, warehouse_id):
    warehouse = get_object_or_404(Warehouse, id=warehouse_id)
    context = {
        'warehouse': warehouse
    }
    return render(request, 'warehouse/view.html', context)