def getPythonTypeName(value):
  return type(value).__name__
def getPythonTypeName2(value, index):
  item = value[index]
  return type(item).__name__
def getPythonValue(value):
  return value