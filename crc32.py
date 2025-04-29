import zlib

for data in ["s4ymt8n8empwibio0ej35h8940s3g9fygh-ztnwxdqxlr6zvl5xa39sssxigjr6d190zi3uvccx93zwrtz2xwgj9pue30t8d0temkxj0q6yi9kdrn5snb2mo2hxi1tsb","kz89xn-jk3690mus813csm3c-4f3p8t0wqcihq6rj14nrn6osc5-3xq25hke2fmfjbz4h25ho00uv2zed7wkubaxtpxz8rwodhiqmzdaa2s5tcmw78"]:
    crc = zlib.crc32(data.encode()) 
    print(f"CRC32: {crc:08X}") 

