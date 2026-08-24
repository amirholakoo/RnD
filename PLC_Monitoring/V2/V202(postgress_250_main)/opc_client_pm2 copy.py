import asyncio
from asyncua import Client, ua

async def main():
   
    url = "opc.tcp://169.254.236.119:4840"          # ←←← DEVICE IP HERE

    async with Client(url=url) as client:


        print("Connected to server")

        # Method 1: Direct NodeId string (recommended - fastest and clearest)
        node_id_str = "ns=4;s=.FAN_BOT_OFST1"
        node = client.get_node(node_id_str)
        
        node_id_str_2 = "ns=4;s=.HDBOX1_OFST1"
        node_2 = client.get_node(node_id_str_2)
        
        node_id_str_3 = "ns=4;s=SPEED_TUNE.BOT_VAC1_ASS"
        node_3 = client.get_node(node_id_str_3)

        # Method 2: 
        # Read the current value
        value = await node.read_value()
        print(f"FAN_BOT_OFST1 value: {value}  (type: {type(value).__name__})")

      
        value = await node_2.read_value()
        print(f"HDBOX1_OFST1 value: {value}  (type: {type(value).__name__})")
        # Optional: Also read other attributes if you want (for debugging)
        display_name = await node.read_display_name()
        print(f"DisplayName: {display_name.Text}")
        
        value = await node_3.read_value()
        print(f"BOT_VAC1_ASS value: {value}  (type: {type(value).__name__})")
        # Optional: Also read other attributes if you want (for debugging)
        display_name = await node.read_display_name()
        print(f"DisplayName: {display_name.Text}")
        
        root = client.nodes.root
        objects = await root.get_child(["0:Objects"])
        print(objects)
        plc = await objects.get_child(["4:PLC1"])
        print(plc)
        ERROR_Node = await plc.get_child(["4:ERROR"])
        print(ERROR_Node)
        ERROR_value = await ERROR_Node.read_value()
        print(ERROR_value)
        


asyncio.run(main())