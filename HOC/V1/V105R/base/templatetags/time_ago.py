from django import template
import time

register = template.Library()

@register.filter
def time_ago(timestamp):
    """Converts timestamp to Persian relative time"""
    if not timestamp:
        return ''
    try:
        now = time.time()
        diff = now - float(timestamp)
        
        if diff < 60:
            return 'همین الان'
        elif diff < 3600:
            minutes = int(diff // 60)
            return f'{minutes} دقیقه قبل'
        elif diff < 86400:
            hours = int(diff // 3600)
            return f'{hours} ساعت قبل'
        elif diff < 604800:
            days = int(diff // 86400)
            return f'{days} روز قبل'
        elif diff < 2592000:
            weeks = int(diff // 604800)
            return f'{weeks} هفته قبل'
        else:
            months = int(diff // 2592000)
            return f'{months} ماه قبل'
    except:
        return ''

