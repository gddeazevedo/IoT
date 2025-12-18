import streamlit as st
import pandas as pd
import gspread
from oauth2client.service_account import ServiceAccountCredentials
import plotly.express as px

# --- CONFIGURAÇÕES ---
NOME_PLANILHA = "Dados_ESP"
NOME_ABA = "Dados_ESP"

st.set_page_config(page_title="Dashboard IoT", layout="wide")

# --- CONEXÃO ---
def get_data():
    try:
        scope = ["https://spreadsheets.google.com/feeds", "https://www.googleapis.com/auth/drive"]
        creds = ServiceAccountCredentials.from_json_keyfile_name("credentials.json", scope)
        client = gspread.authorize(creds)
        sheet = client.open(NOME_PLANILHA).worksheet(NOME_ABA)
        return sheet.get_all_values() 
    except Exception as e:
        st.error(f"Erro na conexão com Google Sheets: {e}")
        return []

# --- PROCESSAMENTO ---
def process_data(data_raw):
    if not data_raw or len(data_raw) < 2:
        return pd.DataFrame()

    df = pd.DataFrame(data_raw[1:]) 

    # --- MAPEAMENTO POSICIONAL ATUALIZADO ---
    # Agora pegamos também Lat (índice 3) e Lon (índice 4)
    # 0=ID, 1=Temp, 2=Umidade, 3=Lat, 4=Lon, 5=Timestamp
    
    if len(df.columns) < 6:
        st.error(f"Erro: Colunas insuficientes.")
        return pd.DataFrame()

    # Seleciona TODAS as colunas relevantes
    df = df.iloc[:, [0, 1, 2, 3, 4, 5]]
    df.columns = ['id', 'temp', 'hum', 'lat', 'lon', 'timestamp']

    # --- LIMPEZA ---
    df['id'] = df['id'].astype(str) # ID como texto para virar categoria

    # Converte Temp, Hum, Lat, Lon (Vírgula -> Ponto)
    cols_numericas = ['temp', 'hum', 'lat', 'lon']
    for col in cols_numericas:
        # Substitui vírgula por ponto e converte para float
        df[col] = df[col].astype(str).str.replace(',', '.').apply(pd.to_numeric, errors='coerce')

    # Timestamp Epoch (Segundos)
    df['timestamp'] = pd.to_numeric(df['timestamp'], errors='coerce')
    df['datahora'] = pd.to_datetime(df['timestamp'], unit='s')
    
    # Ajuste Fuso Brasil (-3h)
    df['datahora'] = df['datahora'] - pd.Timedelta(hours=3)

    return df.dropna(subset=['timestamp', 'temp', 'hum'])

# --- INTERFACE ---
st.title("📡 Dashboard IoT: Monitoramento Unificado")

if st.button("🔄 Atualizar"):
    st.cache_data.clear()

dados_brutos = get_data()
df = process_data(dados_brutos)

if not df.empty:
    
    # --- LÓGICA DE CORES CONSISTENTES (O SEGREDO) ---
    # 1. Pegamos todos os IDs únicos
    unique_ids = df['id'].unique()
    
    # 2. Escolhemos uma paleta de cores forte e distinta (Ex: Plotly Bold)
    palette = px.colors.qualitative.Bold 
    
    # 3. Criamos um dicionário: { 'ID_SENSOR_1': 'CorAzul', 'ID_SENSOR_2': 'CorAmarela' }
    # O uso do modulo (%) garante que se tiver mais sensores que cores, ele recicla as cores
    color_map = {sensor_id: palette[i % len(palette)] for i, sensor_id in enumerate(unique_ids)}
    
    # Mostra quantos sensores ativos temos
    st.info(f"Sensores Monitorados: {', '.join(unique_ids)}")

    # --- PAINEL 1: TEMPERATURA ---
    st.markdown("### 🌡️ Temperatura")
    fig_temp = px.line(
        df, 
        x='datahora', 
        y='temp', 
        color='id', 
        markers=True,
        title="Histórico de Temperatura",
        labels={'datahora': 'Horário', 'temp': 'Temperatura (°C)', 'id': 'Sensor'},
        # AQUI APLICA A COR FIXA:
        color_discrete_map=color_map 
    )
    fig_temp.update_xaxes(tickformat="%d/%m %H:%M")
    st.plotly_chart(fig_temp, use_container_width=True)

    st.divider()

    # --- PAINEL 2: UMIDADE ---
    st.markdown("### 💧 Umidade")
    fig_hum = px.line(
        df, 
        x='datahora', 
        y='hum', 
        color='id', 
        markers=True,
        title="Histórico de Umidade",
        labels={'datahora': 'Horário', 'hum': 'Umidade (%)', 'id': 'Sensor'},
        # AQUI APLICA A MESMA COR FIXA DO OUTRO GRÁFICO:
        color_discrete_map=color_map 
    )
    fig_hum.update_xaxes(tickformat="%d/%m %H:%M")
    st.plotly_chart(fig_hum, use_container_width=True)

    # --- DADOS DETALHADOS (Latitude/Longitude adicionados) ---
    st.markdown("### 📋 Últimas Leituras Detalhadas")
    
    # Prepara a tabela para ficar bonita
    tabela_final = df[['datahora', 'id', 'temp', 'hum', 'lat', 'lon']].copy()
    
    # Ordena do mais recente para o mais antigo
    tabela_final = tabela_final.sort_values(by='datahora', ascending=False)
    
    # Renomeia para ficar bonito na tela
    tabela_final.columns = ['Data/Hora', 'ID Sensor', 'Temp (°C)', 'Umid (%)', 'Latitude', 'Longitude']
    
    st.dataframe(tabela_final, use_container_width=True)

else:
    st.warning("Aguardando dados...")