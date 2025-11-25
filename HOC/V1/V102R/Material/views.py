from django.shortcuts import render
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from .models import Material
import json

@csrf_exempt
def materials(request):
    materials = Material.objects.all()
    context = {
        'materials': materials
    }
    return render(request, 'material/material.html', context)

def add(request):
    if request.method == "POST":
        data = json.loads(request.body)
        material = Material(
            name=data.get("name",''),
            description=data.get("description",''),
        )
        material.save()
        return JsonResponse({"status": "success", "message": "Material added successfully","material": material.id})
    else:
        return JsonResponse({"status": "error", "message": "Invalid request method"}, status=405)

def edit(request, material_id):
    material = Material.objects.get(id=material_id)
    if request.method == "POST":
        data = json.loads(request.body)
        material.name = data.get("name",'')
        material.description = data.get("description",'')
        material.save()
        return JsonResponse({"status": "success", "message": "Material edited successfully","material": material.id})
    else:
        return JsonResponse({"status": "error", "message": "Invalid request method"}, status=405)

def delete(request, material_id):
    material = Material.objects.get(id=material_id)
    material.delete()
    return JsonResponse({"status": "success", "message": "Material deleted successfully"})

def view(request, material_id):
    material = Material.objects.get(id=material_id)
    context = {
        'material': material
    }
    return render(request, 'material/view.html', context)