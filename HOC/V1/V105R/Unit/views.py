from django.shortcuts import render
from .models import Unit
from django.contrib import messages
from django.shortcuts import redirect

# Create your views here.
def units(request):
    units = Unit.objects.filter(Is_Deleted=False)
    return render(request, 'unit/units.html', {'units': units})

def add(request):
    if request.method == 'POST':
        try:
            name = request.POST.get('name')
            description = request.POST.get('description')
            unit = Unit(name=name, description=description)
            unit.save()
            messages.success(request, 'واحد با موفقیت اضافه شد')
            return redirect('units')
        except Exception as e:
            messages.error(request, 'خطا در اضافه شدن واحد: '+str(e))
            return redirect('units')
    return render(request, 'unit/add.html')

def edit(request, unit_id):
    unit = Unit.objects.get(id=unit_id)
    if request.method == 'POST':
        try:
            name = request.POST.get('name')
            description = request.POST.get('description')
            unit.name = name
            unit.description = description
            unit.save()
            messages.success(request, 'واحد با موفقیت ویرایش شد')
            return redirect('edit_unit', unit_id=unit_id)
        except Exception as e:
            messages.error(request, 'خطا در ویرایش واحد: '+str(e))
            return redirect('edit_unit', unit_id=unit_id)
    return render(request, 'unit/edit.html', {'unit': unit})

def delete(request, unit_id):
    unit = Unit.objects.get(id=unit_id)
    try:
        unit.Is_Deleted = True
        unit.save()
        messages.success(request, 'واحد با موفقیت حذف شد')
        return redirect('units')
    except Exception as e:
        messages.error(request, 'خطا در حذف واحد: '+str(e))
        return redirect('units')
    return redirect('units')

def view(request, unit_id):
    unit = Unit.objects.get(id=unit_id)
    return render(request, 'unit/view.html', {'unit': unit})
