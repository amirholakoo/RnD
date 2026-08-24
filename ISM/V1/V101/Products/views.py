from django.shortcuts import render
from django.http import JsonResponse, StreamingHttpResponse
from django.views.decorators.csrf import csrf_exempt
from .models import *
import json, threading, time, requests, re
from django.contrib import messages
from django.shortcuts import redirect
from django.utils import timezone

@csrf_exempt
def add_products_type(request):
    if request.method == "POST":
        body_unicode = request.body.decode('utf-8')
        data = json.loads(body_unicode)
        print(data)
        if ProductsType.objects.filter(name=data["title"]).first():
            return JsonResponse({"status":"error","msg":"از قبل وجود دارد"})
        products = ProductsType(
            name=data["title"],
            description=data["desc"]
        )
        products.save()
        print(data)
    return JsonResponse({"status":"ok"})

@csrf_exempt
def call_api_for_last_roll(request):
    if request.method == "POST":
        body_unicode = request.body.decode('utf-8')
        data = json.loads(body_unicode)
        if not data["machine_id"]:
            return JsonResponse({"status":"error","msg":"ماشین تولید معتبر نیست"})
        
        try:
            machine = ProductsMachine.objects.get(id=int(data["machine_id"]))
            data = requests.post(machine.api).json()
            roll_number = "305"+ ((5 - len(str(data["data"]["roll_number"])))*"0") + str(data["data"]["roll_number"])
            return JsonResponse({"data":roll_number,"breaks":data["data"]["Paper_breaks"],"length":data["data"]["Printed_length"],"status":"ok"})
        except Exception as ex:
            print(ex)
            return JsonResponse({"status":"error","msg":"قادر به ارتباط با plc نیست"})

        print(data)
    return JsonResponse({"status":"ok"})


@csrf_exempt
def add_products(request):
    if request.method == "POST":
        body_unicode = request.body.decode('utf-8')
        data = json.loads(body_unicode)
        if Products.objects.filter(roll_number=data["product_form_roll_number"]).first():
            return JsonResponse({"status":"error","msg":"این ماشین از قبل وجود دارد"})
        products = Products(
            roll_number=data["product_form_roll_number"],
            products_machine= ProductsMachine.objects.get(id=data["products_machine"]),
            width=data["product_form_width"],
            grammage=data["product_form_grammage"],
            length=data["product_form_length"],
            breaks=data["product_form_breaks"],
            type=ProductsType.objects.get(id=data["product_form_type"]),
            profile=int(data["product_form_profile"]),
            description=data["product_form_desc"],
        )
        products.save()
        print(data)
    return JsonResponse({"status":"ok"})

def edit_products(request, id):
    products = products.objects.get(id=id)
    if request.method == "POST":
        try:
            products = products.objects.get(id=id)
            products.License_plate_number = request.POST.get("License_plate_number",'')
            products.description = request.POST.get("description",'')
            products.phone = request.POST.get("phone",'')
            products.driver_name = request.POST.get("driver_name",'')
            products.save()
            messages.success(request,"با موفقیت ویرایش شد")
            return redirect("productss")
        except Exception as e:
            messages.error(request,"خطا در ویرایش products: "+str(e))
            return redirect("edit_products", id=id)
    context = {
        'products': products
    }
    return render(request, 'productss/edit.html', context)

def delete_products(request, id):
        try:
            products = products.objects.get(id=id)
            products.Is_Deleted = True
            products.save()
            messages.success(request,"با موفقیت حذف شد")
            return redirect("productss")
        except Exception as e:
            messages.error(request,"خطا در حذف products: "+str(e))
            return redirect("productss", id=id)

def view_products(request, id):
    products = products.objects.get(id=id)
    context = {
        'products': products
    }
    return render(request, 'productss/view.html', context)



def ProductsListSSE(request):
    """SSE endpoint for streaming products list in real-time (new products + updated product)"""
    def event_stream():
        last_time = 0
        while True:

            products_list = (Products.objects.filter(CreationTimestamp__gte=last_time).values())
            
            if products_list.first():
                result = list(products_list)
                for x in result:
                    x["products_machine_id"] = ProductsMachine.objects.get(id=x["products_machine_id"]).name
                    x["type_id"] = ProductsType.objects.get(id=x["type_id"]).name
                    for t in PRODUCTS_TYPE:
                        if t[0] == x["profile"]:
                            x["profile"] = t[1]
                yield f"data: {json.dumps(result, ensure_ascii=False, default=str)}\n\n"
            
                last_time = result[-1]["CreationTimestamp"]
            time.sleep(2)
    
    response = StreamingHttpResponse(event_stream(), content_type='text/event-stream')
    response['Cache-Control'] = 'no-cache'
    response['X-Accel-Buffering'] = 'no'
    return response


@csrf_exempt
def QR_SettingsView(request):
    body_unicode = request.body.decode('utf-8')
    data = json.loads(body_unicode)
    
    qr_settings = QR_Settings.objects.first()
    if not qr_settings:
        qr_settings = QR_Settings().save()
        return JsonResponse({"status":"error","msg":"error"})
    
    excluded_fields = ",".join(data['excluded_fields'])
    qr_settings.excluded_fields_in_qr = excluded_fields
    qr_settings.custome_qr = data['qr_code_custome']
    qr_settings.save()
    print(data)
    return JsonResponse({"status":"ok"})