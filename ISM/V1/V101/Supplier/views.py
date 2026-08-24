from django.shortcuts import render
from .models import *
from django.views.decorators.csrf import csrf_exempt
import json
from django.contrib import messages
from django.shortcuts import redirect

def suppliers(requests):
    suppliers = Supplier.objects.filter(Is_Deleted=False)
    context = {
        'suppliers': suppliers
    }
    return render(requests, "Supplier/suppliers.html", context)

# Create your views here.
@csrf_exempt
def add(requests):
    if requests.method == "POST":
        try:
            supplier = Supplier(
                name=requests.POST.get("name",''),
                phone=requests.POST.get("phone",''),
                address=requests.POST.get("address",''),
                email=requests.POST.get("email",''),
                description=requests.POST.get("description",''),
            )
            supplier.save()
            messages.success(requests, "تامین کننده با موفقیت اضافه شد")
            return redirect("suppliers")
        except Exception as e:
            messages.error(requests, "خطا در اضافه شدن تامین کننده: "+str(e))
            return redirect("add_supplier")
    return render(requests, "Supplier/add.html")

def edit(requests, supplier_id):
    if requests.method == "POST":
        try:
            supplier = Supplier.objects.get(id=supplier_id)
            supplier.name = requests.POST.get("name",'')
            supplier.phone = requests.POST.get("phone",'')
            supplier.address = requests.POST.get("address",'')
            supplier.email = requests.POST.get("email",'')
            supplier.description = requests.POST.get("description",'')
            supplier.save()
            messages.success(requests, "تامین کننده با موفقیت ویرایش شد")
            return redirect("edit_supplier", supplier_id=supplier_id)
        except Exception as e:
            messages.error(requests, "خطا در ویرایش تامین کننده: "+str(e))
            return redirect("edit_supplier", supplier_id=supplier_id)
    context = {
        'supplier': Supplier.objects.get(id=supplier_id)
    }
    return render(requests, "Supplier/edit.html", context)

def delete(requests, supplier_id):
    try:
        supplier = Supplier.objects.get(id=supplier_id)
        supplier.Is_Deleted = True
        supplier.save()
        messages.success(requests, "تامین کننده با موفقیت حذف شد")
    except Exception as e:
        messages.error(requests, "خطا در حذف تامین کننده: "+str(e))
    return redirect("suppliers")

def view(requests, supplier_id):
    supplier = Supplier.objects.get(id=supplier_id, Is_Deleted=False)
    context = {
        'supplier': supplier
    }
    return render(requests, "Supplier/view.html", context)

