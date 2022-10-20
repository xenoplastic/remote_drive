import asyncio
import websockets

async def hello():
    async with websockets.connect('ws://localhost:6667') as websocket:
        name = input("What's your name? ")     
        await websocket.send(name)
        greeting = await websocket.recv()
        print(f"< {greeting}")


        print(f"> {name}")

       

asyncio.get_event_loop().run_until_complete(hello())
