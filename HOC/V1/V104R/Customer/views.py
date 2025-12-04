from django.shortcuts import render
from .models import *
from django.views.decorators.csrf import csrf_exempt
from django.contrib import messages
from django.shortcuts import redirect

def customers(requests):
    customers = Customer.objects.filter(Is_Deleted=False)
    context = {
        'customers': customers
    }
    return render(requests, "Customer/customers.html", context)

@csrf_exempt
def add(requests):
    if requests.method == "POST":
        try:
            customer = Customer(
                name=requests.POST.get("name",''),
                phone=requests.POST.get("phone",''),
                address=requests.POST.get("address",''),
                email=requests.POST.get("email",''),
                description=requests.POST.get("description",''),
            )
            customer.save()
            messages.success(requests, "مشتری با موفقیت اضافه شد")
            return redirect("customers")
        except Exception as e:
            messages.error(requests, "خطا در اضافه شدن مشتری: "+str(e))
            return redirect("add_customer")
    return render(requests, "Customer/add.html")
def edit(requests, customer_id):
    if requests.method == "POST":
        try:
            customer = Customer.objects.get(id=customer_id)
            customer.name = requests.POST.get("name",'')
            customer.phone = requests.POST.get("phone",'')
            customer.address = requests.POST.get("address",'')
            customer.email = requests.POST.get("email",'')
            customer.description = requests.POST.get("description",'')
            customer.save()
            messages.success(requests, "مشتری با موفقیت ویرایش شد")
            return redirect("edit_customer", customer_id=customer_id)
        except Exception as e:
            messages.error(requests, "خطا در ویرایش مشتری: "+str(e))
            return redirect("edit_customer", customer_id=customer_id)
    context = {
        'customer': Customer.objects.get(id=customer_id)
    }
    return render(requests, "Customer/edit.html", context)

def delete(requests, customer_id):
    try:
        customer = Customer.objects.get(id=customer_id)
        customer.Is_Deleted = True
        customer.save()
        messages.success(requests, "مشتری با موفقیت حذف شد")
    except Exception as e:
        messages.error(requests, "خطا در حذف مشتری: "+str(e))
    return redirect("customers")
        
def view(requests, customer_id):
    customer = Customer.objects.get(id=customer_id, Is_Deleted=False)
    context = {
        'customer': customer
    }
    return render(requests, "Customer/view.html", context)