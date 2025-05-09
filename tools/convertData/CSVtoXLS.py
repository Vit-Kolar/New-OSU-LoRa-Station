import pandas as pd

# Input CSV file path
csv_file = 'data.csv'

# Output Excel file path
xlsx_file = 'data.xlsx'

try:
    # Load CSV with automatic encoding detection
    df = pd.read_csv(csv_file, encoding='utf-8', engine='python')

    # Remove 'raw_json' column if it exists
    if 'raw_json' in df.columns:
        df.drop(columns=['raw_json'], inplace=True)

    # Save to Excel using openpyxl engine
    df.to_excel(xlsx_file, index=False, engine='openpyxl')

    print(f"Done! The file '{xlsx_file}' has been created without the 'raw_json' column.")

except Exception as e:
    print("An error occurred:")
    print(e)

