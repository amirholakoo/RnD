from django.db import models
from ProductsMachine.models import *
import time
import qrcode, os
from PIL import Image, ImageDraw, ImageFont
from django.forms.models import model_to_dict


PRODUCTS_KEY = ["roll_number","products_machine","Width","grammage","Length","breaks","type","profile",]

def generate_qrcode_img(obj):

    data = model_to_dict(obj)

    qrc = qrcode.QRCode(
        version=1,
        error_correction=qrcode.constants.ERROR_CORRECT_L,
        box_size=20,
        border=4,
    )

    qr_setting = QR_Settings.objects.first()

    qr_content = f"roll_number: {obj.roll_number},\nPM: {obj.products_machine.name},\nwidth: {obj.width},\ngrammage: {obj.grammage},\nlength: {obj.length},\n"
    if qr_setting:
        qr_content = ''
        if qr_setting.custome_qr:
            qr_content = qr_setting.custome_qr
            for key in PRODUCTS_KEY:
                qr_content = qr_content.lower().replace(f"*{key.lower()}*",str(data.get(key.lower(), '-')))
        else:
            for key in PRODUCTS_KEY:
                if not key in qr_setting.excluded_fields_in_qr:
                    qr_content += f"{str(key)}: {str(data.get(key.lower(), '-'))},\n"

    qrc.add_data(qr_content)

    qrc.make(fit=True)
    now = time.time()
    qr_name = f"product_{obj.pk}_{now}"
    img = qrc.make_image(fill_color="black", back_color="white")
    if not os.path.exists("media/products"):
        os.makedirs("media/products")
    img.save(F"media/products/{qr_name}.png")
    # image = Image.open(f'media/products/{qr_name}.png')
    # w, height = image.size
    # draw = ImageDraw.Draw(image)
    # code = obj.roll_number
    # font = ImageFont.truetype('static/base/fonts/ttf/Peyda-Bold.ttf', 34)
    # draw.text((160,height - 60), code, font=font)
    # image.save(F"media/products/{qr_name}.png")
    return f"media/products/{qr_name}.png"

class ProductsType(models.Model):
    name = models.CharField(max_length=200, unique=True, verbose_name='نام نوع کاغذ')
    description = models.TextField(blank=True,null=True)
    CreationDateTime = models.DateTimeField(auto_now_add=True, db_index=True, null=True, blank=True)
    LastUpdate = models.DateTimeField(auto_now=True, null=True, blank=True)


PRODUCTS_TYPE = [
        (1, '0'),
        (2, '200'),
        (3, '210-220'),
        (4, '240-250'),
    ]

class Products(models.Model):
    roll_number = models.CharField(max_length=255)
    products_machine = models.ForeignKey(ProductsMachine, on_delete=models.PROTECT, blank=True, null=True, related_name='ProductsMachine', verbose_name='ماشین تولید')
    production_details = models.JSONField(null=True,blank=True)
    width = models.FloatField(blank=True,null=True)
    grammage = models.FloatField(blank=True,null=True)
    length = models.FloatField(blank=True,null=True)
    breaks = models.IntegerField(blank=True,null=True)
    type = models.ForeignKey(ProductsType, on_delete=models.PROTECT, blank=True, null=True, related_name='ProductsType', verbose_name='نوع محصول')
    profile = models.IntegerField(choices=PRODUCTS_TYPE,default=2)
    qr = models.CharField(max_length=100,blank=True,null=True)
    description = models.TextField(blank=True,null=True)
    Is_Deleted = models.BooleanField(default=False)
    CreationTimestamp = models.FloatField(null=True, blank=True)
    CreationDateTime = models.DateTimeField(auto_now_add=True, db_index=True, null=True, blank=True)
    LastUpdate = models.DateTimeField(auto_now=True, null=True, blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationTimestamp:
            self.CreationTimestamp = time.time()
        if not self.qr:
            self.qr = generate_qrcode_img(self)
        super().save(*args, **kwargs)


class QR_Settings(models.Model):
    excluded_fields_in_qr = models.CharField(max_length=200,blank=True,null=True)
    custome_qr = models.TextField(blank=True,null=True)
    CreationTimestamp = models.FloatField(null=True, blank=True)
    CreationDateTime = models.DateTimeField(auto_now_add=True, db_index=True, null=True, blank=True)
    LastUpdate = models.DateTimeField(auto_now=True, null=True, blank=True)
    