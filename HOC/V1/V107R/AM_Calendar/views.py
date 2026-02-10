from django.shortcuts import render, redirect
from django.views.decorators.csrf import csrf_exempt
from django.http import HttpResponse, HttpResponseRedirect, JsonResponse
import jdatetime,locale,platform

def get_fa(year,month,day):
    if platform.system() == 'Windows':
        FA_LOCALE = 'Persian_Iran'
    else:
        FA_LOCALE = 'fa_IR.UTF-8'

    locale.setlocale(locale.LC_ALL, FA_LOCALE)
    L = {'Sat':'شنبه','Sun':'یک شنبه','Mon':'دوشنبه','Tue':'سه شنبه','Wed':'چهار شنبه','Thu':'پنج شنبه','Fri':'جمعه'}
    try:
        response = str(jdatetime.date(year, month, day).strftime("%a"))
    except:
        response = False
    return response if response else ''
    #return L[f'{response}'] if response else ''

def get_d_index(year,month,day):
    year,month,day = int(year), int(month), int(day)
    L = {'Sat':1,'Sun':2,'Mon':3,'Tue':4,'Wed':5,'Thu':6,'Fri':7}
    try:
        response = str(jdatetime.date(year, month, day, locale="en_US.UTF-8").strftime("%a"))
    except:
        response = False
    return L[f'{response}'] if response else ''


def calendar(form=False,request=False,year=False,month=False,Json_response=False):
    NOW = str(jdatetime.datetime.today().strftime("%Y-%m-%d")).split('-')
    #NOW_DAY = jdatetime.datetime.today().strftime("%Y-%m-%d")
    #jdatetime.date.today()).split('-')[1]
    if not year or year== '0':
        year = NOW[0]
        YEARS = [year]
    else:
        YEARS = [year]
    
    if not month or month== '0':
        month = int(NOW[1]) - 1
    else:
        month = int(month) - 1
    calendar = []
    MONTH_NAME = ['فروردین','اردیبهشت','خرداد','تیر','مرداد','شهریور','مهر','آبان','آذر','دی','بهمن','اسفند']
    x=0
    y=int(month)
    i = {'year':f'{YEARS[x]}','active':False,'month':[]}
    i['month'].append({'name':f'{MONTH_NAME[y]}','num':y+1,'active':True,'days':[]})
    if y+1 < 7:
        #str(jdatetime.date(int(YEARS[x]), y+1, z+1, locale="fa_IR").strftime("%a"))
        for z in range(31):
            i['month'][0]['days'].append({'title':f'{get_fa(int(YEARS[x]),y+1,z+1)}','active':False,'reserved':[]})
            if YEARS[x] == NOW[0] and y+1 == int(NOW[1]) and z+1 == int(NOW[2]):
                i['active'] = True
                i['month'][0]['days'][z]['active'] = True
                curent = {'year':[{'name':str(int(YEARS[x]) -1),'active':False},{'name':YEARS[x],'active':True},{'name':str(int(YEARS[x]) +1),'active':False}],'month':y+1}
    else:
        for z in range(30):
            i['month'][0]['days'].append({'title':f'{get_fa(int(YEARS[x]),y+1,z+1)}','active':False,'reserved':[]})
            if YEARS[x] == NOW[0] and y+1 == int(NOW[1]) and z+1 == int(NOW[2]):
                i['active'] = True
                i['month'][0]['days'][z]['active'] = True
                curent = {'year':[{'name':str(int(YEARS[x]) -1),'active':False},{'name':YEARS[x],'active':True},{'name':str(int(YEARS[x]) +1),'active':False}],'month':y+1}
            
    calendar.append(i)
    if form:
        form = True
    if request.GET.get('form') and request.GET.get('form') != 'false':
        form = True
    if not Json_response:
        return calendar,curent,form,year,month
    else:
        #customers = Reservation.objects.filter(year=year).filter(month=month+1).order_by('-id')
        #print(calendar[0]['month'][0]['days'][-1]['title'],'sssss')
        return JsonResponse({'calendar':calendar,'form':form}, safe=False)


def run_calendar(request,year, month):
    return calendar(False,request,year if year != 'false' else False,month,True)