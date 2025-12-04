from django.shortcuts import render
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from .models import *
import json
from django.contrib import messages
from django.shortcuts import redirect

def material_types(request):
    material_types = MaterialType.objects.all()
    context = {
        'material_types': material_types
    }
    return render(request, 'material/material_type.html', context)

def add_material_type(request):
    material_types = MaterialType.objects.all()
    context = {
        'material_types': material_types
    }
    if request.method == "POST":
        try:
            material_type = MaterialType(
                material_type=request.POST.get("material_type"),
                description=request.POST.get("description"),
                supplier_name=request.POST.get("supplier_name"),
            )
            material_type.save()
            messages.success(request, 'نوع ماده با موفقیت افزوده شد')
            return redirect('material_types')
        except Exception as e:
            messages.error(request, 'خطا در افزودن نوع ماده: '+str(e))
            return redirect('add_material_type')
    context = {
        'material_types': material_types
    }
    return render(request, 'material/add_material_type.html', context)

def edit_material_type(request, material_type_id):
    material_type = MaterialType.objects.get(id=material_type_id)
    if request.method == "POST":
        try:
            material_type.material_type = request.POST.get("material_type")
            material_type.supplier_name = request.POST.get("supplier_name")
            material_type.description = request.POST.get("description")
            material_type.save()
            messages.success(request, 'نوع ماده با موفقیت ویرایش شد')
            return redirect('material_types')
        except Exception as e:
            messages.error(request, 'خطا در ویرایش نوع ماده: '+str(e))
            return redirect('edit_material_type', material_type_id=material_type_id)
    context = {
        'material_type': material_type
    }
    return render(request, 'material/edit_material_type.html', context)

def delete_material_type(request, material_type_id):
    try:
        material_type = MaterialType.objects.get(id=material_type_id)
        material_type.Is_Deleted = True
        material_type.save()
        messages.success(request, 'نوع ماده با موفقیت حذف شد')
    except Exception as e:
        messages.error(request, 'خطا در حذف نوع ماده: '+str(e))
    return redirect('material_types')

def view_material_type(request, material_type_id):
    material_type = MaterialType.objects.get(id=material_type_id)
    context = {
        'material_type': material_type
    }
    return render(request, 'material/view_material_type.html', context)

@csrf_exempt
def materials(request):
    materials = Material.objects.all()
    context = {
        'materials': materials
    }
    return render(request, 'material/material.html', context)

def add(request):
    material_types = MaterialType.objects.all()
    context = {
        'material_types': material_types
    }
    if request.method == "POST":
        try:
            material = Material(
                name=request.POST.get("name",''),
                description=request.POST.get("description",''),
                supplier_name=request.POST.get("supplier_name",''),
                material_type=MaterialType.objects.get(id=request.POST.get("material_type",'')),
            )
            material.save()
            messages.success(request, 'material با موفقیت افزوده شد')
            return redirect('materials')
        except Exception as e:
            messages.error(request, 'خطا در افزودن material: '+str(e))
            return redirect('add_material')
    context = {
        'material_types': material_types
    }
    return render(request, 'material/add.html', context)

def edit(request, material_id):
    material = Material.objects.get(id=material_id)
    material_types = MaterialType.objects.all()
    if request.method == "POST":
        try:
            material.name = request.POST.get("name")
            material.description = request.POST.get("description")
            material.supplier_name = request.POST.get("supplier_name")
            material.material_type = MaterialType.objects.get(id=request.POST.get("material_type"))
            material.save()
            messages.success(request, 'material با موفقیت ویرایش شد')
            return redirect('materials')
        except Exception as e:
            messages.error(request, 'خطا در ویرایش material: '+str(e))
            return redirect('edit_material', material_id=material_id)
    context = {
        'material': material,
        'material_types': material_types
    }
    return render(request, 'material/edit.html', context)
def delete(request, material_id):
    material = Material.objects.get(id=material_id)
    if request.method == "POST":
        try:
            material.Is_Deleted = True
            material.save()
            messages.success(request, 'material با موفقیت حذف شد')
            return redirect('materials')
        except Exception as e:
            messages.error(request, 'خطا در حذف material: '+str(e))
            return redirect('delete_material', material_id=material_id)
    return redirect('materials')

def view(request, material_id):
    material = Material.objects.get(id=material_id)
    context = {
        'material': material
    }
    return render(request, 'material/view.html', context)