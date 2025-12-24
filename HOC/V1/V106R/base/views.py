from django.shortcuts import render
from datetime import datetime

def convert_to_jalali(iso_timestamp):
    try:
        dt = datetime.fromisoformat(iso_timestamp.replace('Z', '+00:00'))
        jalali_dt = jdatetime.datetime.fromgregorian(datetime=dt)
        return jalali_dt.strftime('%Y/%m/%d %H:%M:%S')
    except (ValueError, AttributeError):
        return iso_timestamp

def convert_to_unix_timestamp(iso_timestamp):
    try:
        dt = datetime.fromisoformat(iso_timestamp.replace('Z', '+00:00'))
        return int(dt.timestamp())
    except (ValueError, AttributeError, TypeError):
        return iso_timestamp