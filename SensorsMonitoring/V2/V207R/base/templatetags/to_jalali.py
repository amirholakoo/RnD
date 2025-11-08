from django import template
from datetime import datetime
import jdatetime, locale,platform
register = template.Library()
@register.simple_tag
def to_jalali(timestamp,just_day=False):
    if platform.system() == 'Windows':
        FA_LOCALE = 'Persian_Iran'
    else:
        FA_LOCALE = 'fa_IR.UTF-8'

    locale.setlocale(locale.LC_ALL, FA_LOCALE)
    to_date = datetime.fromtimestamp(float(timestamp))
    if not just_day:
        to_jalali = jdatetime.datetime.fromgregorian(datetime=to_date).strftime("%a, %d %b %Y - %H:%M:%S")
    else:
        to_jalali = jdatetime.datetime.fromgregorian(datetime=to_date).strftime("%a, %d %b %Y")
    return f'{to_jalali}'