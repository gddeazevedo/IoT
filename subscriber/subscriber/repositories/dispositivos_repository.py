from ..database.connector import Connector


def create_dispositivo(id_: int, lat: float, long_: float) -> None:
	with Connector() as conn:
		cursor = conn.cursor()
		cursor.execute(f"INSERT INTO dispositivos (id, lat, long) VALUES ({id_}, {lat}, {long_})")
		conn.commit()


def get_dispositivo_by_id(id_: int) -> tuple | None:
	with Connector() as conn:
		cursor = conn.cursor()
		cursor.execute(f"SELECT * FROM dispositivos WHERE id = {id_}")
		dispositivo = cursor.fetchone()
		
		print(dispositivo)

		if dispositivo:
			return dispositivo

		return None
