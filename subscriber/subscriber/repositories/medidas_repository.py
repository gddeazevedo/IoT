from ..database.connector import Connector


def create_medida(temperatura: float, umidade: float, timestamp: int, id_dispositivo: int) -> None:
	with Connector() as conn:
		cursor = conn.cursor()
		cursor.execute(f'''
			INSERT INTO medidas(temperatura, umidade, timestamp, id_dispositivo)
			VALUES ({temperatura}, {umidade}, {timestamp}, {id_dispositivo})
		''')
		conn.commit()
