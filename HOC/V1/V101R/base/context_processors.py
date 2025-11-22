from django.db.models import Q
from .models import VersionControl
def basicdetail(request):
    context = {
        "version":"0.1",
    }
    # ver = VersionControl.objects.all().first()
    # if not ver:
    #     ver = VersionControl.objects.create(version="0.1")
    # context = {
    #     "version":ver.version,
    # }
    return dict(context)