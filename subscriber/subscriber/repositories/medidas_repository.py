from ..database.connector import Connector


def create_medida(temperatura, umidade, timestamp, id_dispositivo):
	with Connector() as conn:
		cursor = conn.cursor()
		cursor.execute(f"insert into medidas(temperatura, umidade, timestamp, id_dispositivo) values ({temperatura}, {umidade}, {timestamp}, {id_dispositivo})")
		conn.commit()
