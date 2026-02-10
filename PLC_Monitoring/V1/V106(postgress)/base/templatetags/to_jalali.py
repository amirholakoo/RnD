from django import template
import jdatetime
from django.utils import timezone

register = template.Library()

@register.simple_tag
def to_jalali(dt, just_day=False):
    """
    Convert a Django datetime (aware) to Jalali date string.
    dt: datetime.datetime object (Django DateTimeField)
    just_day: if True, only show date, otherwise show date and time
    """
    try:
        # Ensure dt is timezone-aware in local time
        dt_local = timezone.localtime(dt)
        # Convert to Jalali
        jdt = jdatetime.datetime.fromgregorian(datetime=dt_local)
        if just_day:
            return jdt.strftime("%a, %d %b %Y")
        else:
            return jdt.strftime("%a, %d %b %Y - %H:%M:%S")
    except Exception:
        return ""
