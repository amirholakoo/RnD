from django.shortcuts import render
from django.http import HttpResponse,JsonResponse
import requests, time, json, jdatetime, datetime
import platform, locale
# Build paths inside the project like this: BASE_DIR / 'subdir'.

if platform.system() == 'Windows':
    FA_LOCALE = 'Persian_Iran'
else:
    FA_LOCALE = 'fa_IR.UTF-8'
locale.setlocale(locale.LC_ALL, FA_LOCALE)
# License plate recognition port: 4001
# Face recognition port: 3001
MAIN_URL = "http://172.16.9.2"
LICENCE_PLATE_RECOGNIZING_PORT = 4001
FACE_RECOGNIZING_PORT = 3001
Plates = {
    'الف' : 'alef',
    'ب' : 'b',
    'ج' : 'j',
    'ل' : 'l',
    'م' : 'm',
    'ن' : 'n',
    'ق' : 'q',
    'و' : 'v',
    'ه': 'h',
    'ی' : 'y',
    'د' : 'd',
    'س': 's',
    'ص' : 'sad',
    'معلول' : 'malol',
    'ت' : 't',
    'ط' : 'ta',
    'ع' : 'ein',
    'D' : 'diplomat',
    'S' : 'siyasi',
    'پ' : 'p',
    'تشریفات' : 'tashrifat',
    'ث': 'the',
    'ز': 'ze',
    'ش' : 'she',
    'ف': 'fe',
    'ک' : 'kaf',
    'گ' : 'gaf',
    '#' : '#',
}
def en_plate_to_fa(plate):
    global Plates
    plate_letter = False

    # find letters in plate string
    for x in plate:
        try:
            x = int(x)
            continue
        except:
            if not plate_letter:
                plate_letter = x
            else:
                plate_letter += x

    for k,v in Plates.items():
        if v == plate_letter:
            plate = plate.replace(v,k).replace(plate[-2:],f"ایران{plate[-2:]}")

    return plate


def GET_TOKEN():
    USER_AUTHORIZATION_TOKEN = False
    response = requests.post(url=f"{MAIN_URL}:{LICENCE_PLATE_RECOGNIZING_PORT}/users/auth",data={"username":"admin",
                                                                                                 "password":"admin@123"})
    if response.status_code == 200:
        data = response.json()
        USER_AUTHORIZATION_TOKEN = data["user"]["token"]

    return USER_AUTHORIZATION_TOKEN



def ReportsPlate(startDate,endDate):
    print("start to get report of plates")
    print("get token ...")
    token = GET_TOKEN()
    if not token:
        return HttpResponse("user token faild!")
    print(f"token: f{token}")
    print("get report ...")
    headers = {
        "Authorization": f"Bearer {token}"
    }
    data={
        'commonSearchParams': {
            'startDate': startDate,
            'endDate': endDate,
            # 'startDate': 1776457800000,
            # 'endDate': 1776544199999,
            'all_camera_ids': [
                1,
                2,
                3,
                4
            ],
            'camera_ids': [1,2],
            'memberId': -1,
            'twoStepVerificationStatus': -1,
            'cameraId': -1,
            'permission': -1,
            'phoneNumber': '',
            'companyName': '',
            'direction': -1,
            'plate_types': [
                0,
                1,
                2,
                3,
                4,
                5,
                6,
                7
            ],
            'price_types': [],
            'vehicle_classes': [],
            'vehicle_types': [],
            'update_statuses': [],
            'corrupt_statuses': [],
            'vehicle_colors': [],
            'export_options': [
                0,
                1,
                2,
                3,
                4,
                5,
                6,
                8,
                9,
                10,
                11,
                12
            ],
            'parking_statuses': [],
            'min_vehicle_class_conf': 0,
            'min_vehicle_type_conf': 0,
            'min_vehicle_color_conf': 0,
            'min_vision_speed': -180,
            'max_vision_speed': 180,
            'show_vision_speed': False,
            'show_None_speed': True,
            'min_lane': 0,
            'max_lane': 5,
            'min_ocr': 0,
            'max_ocr': 1,
            'show_lane': True,
            'min_radar_speed': 0,
            'max_radar_speed': 180,
            'show_radar_speed': False,
            'search_not_legible': False,
            'export_crop_image': True,
            'export_full_image': False,
            'max_count': 1000,
            'can_delete': False,
            'can_verify': True,
            'show_legible_option': True,
            'is_verified': None,
            'is_rejected': None,
            'plate_search_list': [],
            'verify_status': 0,
            'owner_id': -1,
            'permissionId': [],
            'allMemberIds': [],
            'description_list': [],
            'description_member_list': [],
            'plate': [
                None,
                None,
                None,
                None,
                None,
                None,
                None,
                None
            ]
        },
            'page': 1,
            'pageSize': 50000,
    }
    response = requests.post(url=f"{MAIN_URL}:{LICENCE_PLATE_RECOGNIZING_PORT}/reportsPlate",headers=headers,json=data)
    data=False
    if response.status_code == 200:
        data = json.loads(response.text)["results"]
    
    #return JsonResponse(data, safe=False)
    
    data = [{'plate':en_plate_to_fa(x["plate_char"]),'name':f'{x["first_name"]} {x["last_name"]}','time':jdatetime.datetime.fromtimestamp(float(x['time_epoch_ms'])/1000).strftime('%a, %d %b %Y %H:%M')} for x in data]
    
    # return HttpResponse(
    #     f"<h2 style='direction:rtl'>{x}<h2>" for x in data
    # )
    return data


# ReportsPlate((time.time() - 28800) * 1000,(time.time()) * 1000)

def Recommended_Plate(request):
    print("=========================")
    day_ago = request.GET.get("ago",1)
    print(day_ago)
    data = ReportsPlate((time.time() - (int(day_ago) * 24*60*60)) * 1000,(time.time()) * 1000)
    try:
        return JsonResponse(data,safe=False)
    except Exception as ex:
        print(ex)
        response = {
            "data": [],
        }
        return JsonResponse(response,safe=False)
    
