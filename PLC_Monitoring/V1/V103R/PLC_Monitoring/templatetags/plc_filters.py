from django import template
from PLC_Monitoring.models import PLC_Keys

register = template.Library()

@register.filter
def get_key_name(key):
    """Get fa_name or name from PLC_Keys by key, fallback to key itself"""
    try:
        plc_key = PLC_Keys.objects.filter(key=key).first()
        if plc_key:
            return plc_key.fa_name or plc_key.name or key
    except:
        pass
    return key
