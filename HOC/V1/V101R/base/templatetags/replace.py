from django import template

register = template.Library()

@register.simple_tag
def replace(price):
    if price == None or not price:
        price = 0000
    return f'{price:,}' 