from django.shortcuts import render
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from .models import *
import json, threading, time, requests, re
from django.contrib import messages
from django.shortcuts import redirect


@csrf_exempt
def add_machine(request):
    if request.method == "POST":
        body_unicode = request.body.decode('utf-8')
        data = json.loads(body_unicode)
        if ProductsMachine.objects.filter(name=data["machine_name"]).first():
            return JsonResponse({"status":"error","msg":"این ماشین از قبل وجود دارد"})
        machine = ProductsMachine(
            name=data["machine_name"],
            api=data["machine_api"],
        )
        machine.save()
        print(data)
    return JsonResponse({"status":"ok"})
        # try:
        #     machine = ProductsMachine(
        #         License_plate_number=request.POST.get("License_plate_number",''),
        #         description=request.POST.get("description",''),
        #         phone=request.POST.get("phone",''),
        #         driver_name=request.POST.get("driver_name",''),
        #         location=request.POST.get("location",''),
        #     )
        #     machine.save()
        #     messages.success(request,"با موفقیت اضافه شد")
        #     return redirect("machines")
        # except Exception as e:
        #     messages.error(request,"خطا در اضافه شدن machine: "+str(e))
        #     return redirect("add_machine")


def edit_machine(request, id):
    machine = machine.objects.get(id=id)
    if request.method == "POST":
        try:
            machine = machine.objects.get(id=id)
            machine.License_plate_number = request.POST.get("License_plate_number",'')
            machine.description = request.POST.get("description",'')
            machine.phone = request.POST.get("phone",'')
            machine.driver_name = request.POST.get("driver_name",'')
            machine.save()
            messages.success(request,"با موفقیت ویرایش شد")
            return redirect("machines")
        except Exception as e:
            messages.error(request,"خطا در ویرایش machine: "+str(e))
            return redirect("edit_machine", id=id)
    context = {
        'machine': machine
    }
    return render(request, 'machines/edit.html', context)

def delete_machine(request, id):
        try:
            machine = machine.objects.get(id=id)
            machine.Is_Deleted = True
            machine.save()
            messages.success(request,"با موفقیت حذف شد")
            return redirect("machines")
        except Exception as e:
            messages.error(request,"خطا در حذف machine: "+str(e))
            return redirect("machines", id=id)

def view_machine(request, id):
    machine = machine.objects.get(id=id)
    context = {
        'machine': machine
    }
    return render(request, 'machines/view.html', context)
