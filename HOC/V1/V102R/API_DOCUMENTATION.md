# ISM Inventory Management System - API Documentation

## 🔐 Authentication

**Current Status:** No authentication required for API endpoints

- All API endpoints are currently open and do not require authentication tokens
- CSRF protection is disabled on API endpoints (`@csrf_exempt`)
- CORS is enabled for `http://localhost:5173` and `http://127.0.0.1:5173`

⚠️ **Security Recommendation:** For production use, implement authentication using Django REST Framework with token authentication or JWT.

---

## 📡 Base URL

```
http://your-server-ip:8000/myapp/
```

For local development:
```
http://localhost:8000/myapp/
```

---

## 📊 Product & Inventory APIs

### 1. Get Products Report (In-Stock Products)

**Endpoint:** `/api/reportProducts`  
**Method:** `POST`  
**Description:** Retrieve all in-stock products with detailed information including dimensions, grade, and location.

**Query Parameters:**
- `filter` (required): Time filter for data
  - Values: `year`, `month`, `week`, `day`

**Response Fields:**
- `date`, `location`, `width`, `reel_number`, `gsm`, `length`, `grade`, `breaks`, `status`, `receive_date`, `last_date`, `comments`, `qr_code`, `profile_name`, `shipment_id_id`, `username`, `logs`

**Real Example:**

```python
import requests
import json

# Get products from the last month
url = "http://localhost:8000/myapp/api/reportProducts"
params = {"filter": "month"}

response = requests.post(url, params=params)
data = response.json()

print(f"Status: {data['status']}")
print(f"Title: {data['title']}")
print(f"Number of products: {len(data['values'])}")
print(f"\nFirst product:")
print(json.dumps(data['values'][0], indent=2))

# Access specific fields
for product in data['values']:
    print(f"Reel: {product['reel_number']}, Width: {product['width']}, Grade: {product['grade']}, Location: {product['location']}")
```

**cURL Example:**
```bash
curl -X POST "http://localhost:8000/myapp/api/reportProducts?filter=month"
```

**Sample Response:**
```json
{
  "status": "success",
  "title": "لیست محصولات",
  "fields": ["date", "location", "width", "reel_number", "gsm", "length", "grade", "breaks", "status", "receive_date"],
  "values": [
    {
      "id": 1,
      "date": "1403-09-15 14:30",
      "location": "Anbar_Salon_Tolid",
      "width": 120,
      "reel_number": "1001",
      "gsm": 80,
      "length": 5000,
      "grade": "A",
      "breaks": 2,
      "status": "In-stock",
      "receive_date": "1403-09-15 14:30"
    }
  ]
}
```

---

### 2. Get Products by Location (Aggregated View)

**Endpoint:** `/ProductsPage/`  
**Method:** `POST`  
**Description:** Get aggregated product counts grouped by width and location.

**Query Parameters:**
- `filter` (required): `year`, `month`, `week`, `day`

**Real Example:**

```python
import requests

url = "http://localhost:8000/myapp/ProductsPage/"
params = {"filter": "week"}

response = requests.post(url, params=params)
data = response.json()

# Display inventory summary
for item in data['values']:
    print(f"Location: {item['location']:30} | Width: {item['width']:4} cm | Quantity: {item['quantity']:3} | Status: {item['status']}")
```

**Sample Response:**
```json
{
  "status": "success",
  "title": "لیست محصولات",
  "fields": ["width", "location", "quantity", "status"],
  "values": [
    {
      "width": 120,
      "location": "Anbar_Salon_Tolid",
      "quantity": 45,
      "status": "In-stock"
    },
    {
      "width": 100,
      "location": "Anbar_Sangin",
      "quantity": 32,
      "status": "In-stock"
    }
  ]
}
```

---

### 3. Get Raw Material Inventory

**Endpoint:** `/api/reportRawMaterial`  
**Method:** `POST`  
**Description:** Retrieve raw material inventory across all warehouses with quantities.

**Query Parameters:**
- `filter` (required): `year`, `month`, `week`, `day`

**Real Example:**

```python
import requests

url = "http://localhost:8000/myapp/api/reportRawMaterial"
params = {"filter": "month"}

response = requests.post(url, params=params)
data = response.json()

print("\n📦 Raw Material Inventory:")
print(f"{'Supplier':20} | {'Material':20} | {'Type':15} | {'Location':20} | {'Qty':5}")
print("-" * 85)

for material in data['values']:
    print(f"{material['supplier_name']:20} | {material['material_name']:20} | {material['material_type']:15} | {material['location']:20} | {material['quantity']:5}")
```

**Sample Response:**
```json
{
  "status": "success",
  "title": "لیست مواد اولیه",
  "fields": ["unit", "location", "quantity", "supplier_name", "material_type", "material_name"],
  "values": [
    {
      "unit": "kg",
      "location": "Anbar_Akhal",
      "quantity": 150,
      "supplier_name": "Supplier A",
      "material_type": "Chemical",
      "material_name": "Bleach"
    }
  ]
}
```

---

### 4. Get Purchases Report (with QC Data)

**Endpoint:** `/api/reportPurchases`  
**Method:** `POST`  
**Description:** Get purchase orders including quality assessment and penalty information.

**Query Parameters:**
- `filter` (required): `year`, `month`, `week`, `day`

**Real Example:**

```python
import requests

url = "http://localhost:8000/myapp/api/reportPurchases"
params = {"filter": "week"}

response = requests.post(url, params=params)
data = response.json()

print("\n🔍 QC Report from Purchases:")
for purchase in data['values']:
    print(f"\nPurchase ID: {purchase['id']}")
    print(f"  Supplier: {purchase['supplier_name']}")
    print(f"  Material: {purchase['material_name']}")
    print(f"  Quality: {purchase['quality']}")
    print(f"  Penalty: {purchase['penalty']}")
    print(f"  Quantity: {purchase['quantity']} {purchase['unit']}")
    print(f"  Status: {purchase['status']}")
```

---

### 5. Get Shipments Report

**Endpoint:** `/api/reportShipment`  
**Method:** `POST`  
**Description:** Get active shipments (excluding delivered and cancelled).

**Query Parameters:**
- `filter` (required): `year`, `month`, `week`, `day`

**Real Example:**

```python
import requests

url = "http://localhost:8000/myapp/api/reportShipment"
params = {"filter": "day"}

response = requests.post(url, params=params)
data = response.json()

print("\n🚛 Active Shipments:")
for shipment in data['values']:
    print(f"License: {shipment['license_number']} | Type: {shipment['shipment_type']} | Status: {shipment['status']} | Location: {shipment['location']}")
```

---

### 6. Get Consumption Report

**Endpoint:** `/api/reportConsumption`  
**Method:** `POST`  
**Description:** Track material consumption by production.

**Query Parameters:**
- `filter` (required): `year`, `month`, `week`, `day`

**Real Example:**

```python
import requests

url = "http://localhost:8000/myapp/api/reportConsumption"
params = {"filter": "month"}

response = requests.post(url, params=params)
data = response.json()

print("\n📉 Material Consumption:")
for item in data['values']:
    print(f"Profile: {item['profile_name']} | Material: {item['material_name']} | Supplier: {item['supplier_name']} | Status: {item['status']}")
```

---

## 🔍 Query APIs

### 7. Get Available Widths in Warehouse

**Endpoint:** `/api/getWidths`  
**Method:** `GET`  
**Description:** Get all distinct widths available in a specific warehouse.

**Query Parameters:**
- `anbar_location` (required): Warehouse name (e.g., "Anbar_Salon_Tolid")

**Real Example:**

```python
import requests

url = "http://localhost:8000/myapp/api/getWidths"
params = {"anbar_location": "Anbar_Salon_Tolid"}

response = requests.get(url, params=params)
data = response.json()

print(f"Available widths: {data['widths']}")
```

---

### 8. Get Reel Numbers by Width

**Endpoint:** `/api/getReelNumbersByWidthAndStatus`  
**Method:** `GET`  
**Description:** Get reel numbers for a specific width in a warehouse.

**Query Parameters:**
- `anbar_location` (required): Warehouse name
- `width` (required): Width in cm

**Real Example:**

```python
import requests

url = "http://localhost:8000/myapp/api/getReelNumbersByWidthAndStatus"
params = {
    "anbar_location": "Anbar_Salon_Tolid",
    "width": 120
}

response = requests.get(url, params=params)
data = response.json()

print(f"Reels with width 120cm: {data['reel_numbers']}")
```

---

### 9. Get Supplier Names

**Endpoint:** `/api/getSupplierNames`  
**Method:** `GET`  
**Description:** Get list of active suppliers.

**Real Example:**

```python
import requests

url = "http://localhost:8000/myapp/api/getSupplierNames"
response = requests.get(url)
data = response.json()

print(f"Active suppliers: {data['supplier_names']}")
```

---

### 10. Get Material Names by Supplier

**Endpoint:** `/api/getMaterialNames`  
**Method:** `GET`  
**Description:** Get materials provided by a specific supplier.

**Query Parameters:**
- `supplier_name` (required): Supplier name

**Real Example:**

```python
import requests

url = "http://localhost:8000/myapp/api/getMaterialNames"
params = {"supplier_name": "Supplier A"}

response = requests.get(url, params=params)
data = response.json()

print(f"Materials from Supplier A: {data['material_names']}")
```

---

### 11. Get All Warehouse Names

**Endpoint:** `/api/getAnbarTableNames`  
**Method:** `GET`  
**Description:** Get list of all warehouse locations.

**Real Example:**

```python
import requests

url = "http://localhost:8000/myapp/api/getAnbarTableNames"
response = requests.get(url)
data = response.json()

print(f"Available warehouses: {data['data']}")
```

---

## 🔬 Complete Lab Integration Example

### Scenario: Lab System Queries ISM for QC Data

```python
import requests
import pandas as pd
from datetime import datetime

class ISM_API_Client:
    """Client for ISM Inventory Management System API"""
    
    def __init__(self, base_url="http://localhost:8000"):
        self.base_url = base_url
        self.api_base = f"{base_url}/myapp"
    
    def get_products_for_qc(self, time_filter="week"):
        """Get products that need QC inspection"""
        url = f"{self.api_base}/api/reportProducts"
        params = {"filter": time_filter}
        
        response = requests.post(url, params=params)
        data = response.json()
        
        if data['status'] == 'success':
            return data['values']
        return []
    
    def get_purchases_with_quality(self, time_filter="month"):
        """Get purchase data with quality information"""
        url = f"{self.api_base}/api/reportPurchases"
        params = {"filter": time_filter}
        
        response = requests.post(url, params=params)
        data = response.json()
        
        if data['status'] == 'success':
            return data['values']
        return []
    
    def get_inventory_by_location(self, location):
        """Get inventory for specific warehouse"""
        # First get available widths
        url = f"{self.api_base}/api/getWidths"
        params = {"anbar_location": location}
        
        response = requests.get(url, params=params)
        return response.json()
    
    def generate_qc_report(self):
        """Generate comprehensive QC report"""
        print("=" * 80)
        print("ISM INVENTORY SYSTEM - QC REPORT")
        print(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print("=" * 80)
        
        # Get products
        products = self.get_products_for_qc("week")
        print(f"\n📦 Products Received This Week: {len(products)}")
        
        # Get purchases with quality data
        purchases = self.get_purchases_with_quality("month")
        print(f"\n💰 Purchases This Month: {len(purchases)}")
        
        # Quality analysis
        if purchases:
            df = pd.DataFrame(purchases)
            print("\n🔍 Quality Analysis:")
            print(df[['supplier_name', 'material_name', 'quality', 'penalty', 'quantity']].to_string())
        
        # Get raw materials
        raw_url = f"{self.api_base}/api/reportRawMaterial"
        raw_response = requests.post(raw_url, params={"filter": "month"})
        raw_data = raw_response.json()
        
        if raw_data['status'] == 'success':
            print(f"\n📋 Raw Materials in Stock: {len(raw_data['values'])}")
            df_raw = pd.DataFrame(raw_data['values'])
            print(df_raw[['supplier_name', 'material_name', 'location', 'quantity']].to_string())


# Usage Example
if __name__ == "__main__":
    # Initialize client
    client = ISM_API_Client(base_url="http://localhost:8000")
    
    # Generate QC report
    client.generate_qc_report()
    
    # Get specific product data
    products = client.get_products_for_qc("day")
    print(f"\n📊 Today's products requiring QC: {len(products)}")
    
    for product in products[:5]:  # Show first 5
        print(f"  Reel: {product['reel_number']} | Width: {product['width']} | Grade: {product['grade']} | Location: {product['location']}")
```

---

## 📤 Export to Excel

**Endpoint:** `/api/generateExcelReport`  
**Method:** `POST`  
**Description:** Generate and download Excel report for any model.

**Query Parameters:**
- `model_name` (required): One of: `shipments`, `sales`, `purchases`, `rawMaterial`, `products`, `consumption`, `alerts`

**Real Example:**

```python
import requests

url = "http://localhost:8000/myapp/api/generateExcelReport"
params = {"model_name": "products"}

response = requests.post(url, params=params)

# Save the Excel file
with open("products_report.xlsx", "wb") as f:
    f.write(response.content)

print("Excel report saved as products_report.xlsx")
```

---

## 🔔 Alerts API

**Endpoint:** `/api/reportAlert`  
**Method:** `POST`  
**Description:** Get system alerts (inventory warnings, errors, etc.).

**Query Parameters:**
- `filter` (required): `year`, `month`, `week`, `day`

**Real Example:**

```python
import requests

url = "http://localhost:8000/myapp/api/reportAlert"
params = {"filter": "day"}

response = requests.post(url, params=params)
data = response.json()

print("\n⚠️ Today's Alerts:")
for alert in data['values']:
    print(f"[{alert['date']}] {alert['status']}: {alert['message']}")
```

---

## 🛠️ Error Handling

All APIs return consistent error responses:

```json
{
  "status": "error",
  "message": "Error description"
}
```

Or for validation errors:

```json
{
  "status": "error",
  "errors": [
    {"status": "error", "message": "Field is required"}
  ]
}
```

**Python Error Handling Example:**

```python
import requests

def safe_api_call(url, method="GET", params=None):
    try:
        if method == "GET":
            response = requests.get(url, params=params, timeout=10)
        else:
            response = requests.post(url, params=params, timeout=10)
        
        response.raise_for_status()
        data = response.json()
        
        if data.get('status') == 'error':
            print(f"API Error: {data.get('message')}")
            return None
        
        return data
    
    except requests.exceptions.Timeout:
        print("Request timed out")
    except requests.exceptions.ConnectionError:
        print("Cannot connect to server")
    except requests.exceptions.RequestException as e:
        print(f"Request failed: {e}")
    
    return None

# Usage
data = safe_api_call(
    "http://localhost:8000/myapp/api/reportProducts",
    method="POST",
    params={"filter": "month"}
)

if data:
    print(f"Retrieved {len(data['values'])} products")
```

---

## 📚 Complete Python Module

Save this as `ism_api.py` in your lab project:

```python
"""
ISM API Client - Interface for ISM Inventory Management System

Usage:
    from ism_api import ISMAPI
    
    api = ISMAPI("http://your-server:8000")
    products = api.get_products("month")
    purchases = api.get_purchases_qc("week")
"""

import requests
from typing import List, Dict, Optional
import json


class ISMAPI:
    """Client for ISM Inventory Management System API"""
    
    def __init__(self, base_url: str = "http://localhost:8000"):
        self.base_url = base_url.rstrip('/')
        self.api_base = f"{self.base_url}/myapp"
        self.timeout = 30
    
    def _request(self, endpoint: str, method: str = "GET", params: Optional[Dict] = None) -> Optional[Dict]:
        """Make API request with error handling"""
        url = f"{self.api_base}{endpoint}"
        
        try:
            if method.upper() == "GET":
                response = requests.get(url, params=params, timeout=self.timeout)
            else:
                response = requests.post(url, params=params, timeout=self.timeout)
            
            response.raise_for_status()
            return response.json()
        
        except requests.exceptions.RequestException as e:
            print(f"API Request Error: {e}")
            return None
    
    # Product APIs
    def get_products(self, time_filter: str = "month") -> List[Dict]:
        """Get in-stock products"""
        data = self._request("/api/reportProducts", "POST", {"filter": time_filter})
        return data.get('values', []) if data else []
    
    def get_products_aggregated(self, time_filter: str = "month") -> List[Dict]:
        """Get aggregated product counts by location and width"""
        data = self._request("/ProductsPage/", "POST", {"filter": time_filter})
        return data.get('values', []) if data else []
    
    # Raw Material APIs
    def get_raw_materials(self, time_filter: str = "month") -> List[Dict]:
        """Get raw material inventory"""
        data = self._request("/api/reportRawMaterial", "POST", {"filter": time_filter})
        return data.get('values', []) if data else []
    
    # Purchase APIs (QC Data)
    def get_purchases_qc(self, time_filter: str = "month") -> List[Dict]:
        """Get purchase orders with quality/penalty data"""
        data = self._request("/api/reportPurchases", "POST", {"filter": time_filter})
        return data.get('values', []) if data else []
    
    # Shipment APIs
    def get_shipments(self, time_filter: str = "day") -> List[Dict]:
        """Get active shipments"""
        data = self._request("/api/reportShipment", "POST", {"filter": time_filter})
        return data.get('values', []) if data else []
    
    # Consumption APIs
    def get_consumption(self, time_filter: str = "month") -> List[Dict]:
        """Get consumption records"""
        data = self._request("/api/reportConsumption", "POST", {"filter": time_filter})
        return data.get('values', []) if data else []
    
    # Query APIs
    def get_warehouses(self) -> List[str]:
        """Get list of all warehouse names"""
        data = self._request("/api/getAnbarTableNames", "GET")
        return data.get('data', []) if data else []
    
    def get_widths_in_warehouse(self, warehouse: str) -> List[int]:
        """Get available widths in a warehouse"""
        data = self._request("/api/getWidths", "GET", {"anbar_location": warehouse})
        return data.get('widths', []) if data else []
    
    def get_reels_by_width(self, warehouse: str, width: int) -> List[str]:
        """Get reel numbers for specific width"""
        params = {"anbar_location": warehouse, "width": width}
        data = self._request("/api/getReelNumbersByWidthAndStatus", "GET", params)
        return data.get('reel_numbers', []) if data else []
    
    def get_suppliers(self) -> List[str]:
        """Get active supplier names"""
        data = self._request("/api/getSupplierNames", "GET")
        return data.get('supplier_names', []) if data else []
    
    def get_materials_by_supplier(self, supplier: str) -> List[str]:
        """Get materials from specific supplier"""
        data = self._request("/api/getMaterialNames", "GET", {"supplier_name": supplier})
        return data.get('material_names', []) if data else []
    
    # Alert APIs
    def get_alerts(self, time_filter: str = "day") -> List[Dict]:
        """Get system alerts"""
        data = self._request("/api/reportAlert", "POST", {"filter": time_filter})
        return data.get('values', []) if data else []
    
    # Export
    def download_excel_report(self, model_name: str, output_file: str):
        """Download Excel report for a model"""
        url = f"{self.api_base}/api/generateExcelReport"
        params = {"model_name": model_name}
        
        try:
            response = requests.post(url, params=params, timeout=self.timeout)
            response.raise_for_status()
            
            with open(output_file, 'wb') as f:
                f.write(response.content)
            
            print(f"Report saved to {output_file}")
            return True
        
        except Exception as e:
            print(f"Download failed: {e}")
            return False


# Example usage
if __name__ == "__main__":
    api = ISMAPI("http://localhost:8000")
    
    # Get products for QC
    products = api.get_products("week")
    print(f"Products this week: {len(products)}")
    
    # Get QC data from purchases
    purchases = api.get_purchases_qc("month")
    print(f"\nPurchases with QC data:")
    for p in purchases[:3]:
        print(f"  - {p['material_name']}: Quality={p['quality']}, Penalty={p['penalty']}")
    
    # Get inventory by location
    warehouses = api.get_warehouses()
    print(f"\nWarehouses: {warehouses}")
    
    # Download report
    api.download_excel_report("products", "products_report.xlsx")
```

---

## 🚀 Quick Start Guide

### 1. Install Requirements

```bash
pip install requests pandas
```

### 2. Test Connection

```python
import requests

url = "http://localhost:8000/myapp/api/getSupplierNames"
response = requests.get(url)

if response.status_code == 200:
    print("✅ Connection successful!")
    print(f"Suppliers: {response.json()['supplier_names']}")
else:
    print("❌ Connection failed")
```

### 3. Get QC Data

```python
import requests

url = "http://localhost:8000/myapp/api/reportPurchases"
response = requests.post(url, params={"filter": "month"})
data = response.json()

for purchase in data['values']:
    print(f"Quality: {purchase['quality']}, Penalty: {purchase['penalty']}")
```

---

## 📞 Support

For issues or questions:
- Check server is running: `python manage.py runserver`
- Verify URL is correct: `http://localhost:8000/myapp/`
- Check firewall settings if accessing remotely

---

## 🔒 Security Notes for Production

**Current Setup (Development):**
- ✅ No authentication required
- ✅ CSRF disabled for API endpoints
- ✅ CORS enabled for localhost

**Recommended for Production:**
1. Implement Django REST Framework with token authentication
2. Enable HTTPS/TLS
3. Add rate limiting
4. Implement API key authentication
5. Add request logging
6. Enable CSRF protection
7. Restrict CORS to specific domains

---

Last Updated: 2025-01-07
