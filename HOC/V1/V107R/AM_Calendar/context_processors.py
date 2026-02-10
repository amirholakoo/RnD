from .views import calendar

def calendar_details(request):
    cal,curent,form,year,month = calendar(False,request)
    context = {
        'calendar': cal,
        'curent': curent,
    }
    return context