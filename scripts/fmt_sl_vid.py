from inc_noesis import *
from timeit import default_timer as timer


def registerNoesisTypes():
    handle = noesis.register("Locoland(Steamland) sprites", ".vid")
    
    noesis.setHandlerTypeCheck(handle, locCheckType)
    noesis.setHandlerLoadRGBA(handle, locLoadRGBA)
    
    return 1

    
class imageSprite:
    def __init__(self, width, height, data):  
        self.width = width
        self.height = height
        self.data = data        
        
    
class LLSpriteArchive:
    def __init__(self, reader):
        self.reader = reader    
        self.width = 0
        self.height = 0
        self.type0 = 0
        self.spriteCount = 0
        self.paletteSize = 0
        self.sprites = []
        
    def parseHeader(self):
        self.magic = self.reader.readUInt() 
        if self.magic != 542328146: # "RES "
            return 0
  
        self.reader.seek(4, NOESEEK_REL) 
        
        vid = self.reader.readUInt()
        head = self.reader.readUInt()
        if vid != 541346134 and head != 1145128264: # "VID HEAD"
            return 0   
                      
        self.reader.seek(20, NOESEEK_REL)
        self.type0 = self.reader.readShort() 
        
        self.reader.seek(2, NOESEEK_REL) 
        self.spriteCount = self.reader.readShort() 
        
        self.width = self.reader.readShort()  
        self.height = self.reader.readShort()  
        
        return 1
     
    def palettedToRGBA32(self, indexes):
        imageData = rapi.imageDecodeRawPal(indexes, self.palette, self.width, \
            self.height, 8, "r8g8b8")        
    
        return imageData
        
        
    def getRGBData(self, filereader, width, height, type):        
        size = filereader.readUInt()
        imageData = filereader.readBytes(size)
              
        if type == 0: # image                   
            formats = ["b4g4r4a4", "b5g6r5"] 
            if self.spriteCount == 1: 
                format = formats[1]
            else:
                format = formats[0]            
            rgbData = rapi.imageDecodeRaw(imageData, width, height, format)
        else: # mask
            rgbData = rapi.imageDecodeRaw(imageData, width, height, "r12b4")          
            #rgbData = bytearray()
            #memBuffer = memoryview(imageData)
            #alpha = (255).to_bytes(1, byteorder = "little")            
            #for i in range(0, size, 2):
                #color = memBuffer[i]
                #rgbData += color
                #rgbData += color
                #rgbData += color
                #rgbData += alpha
         
        return rgbData
     
     
    def readSURFSection(self, filereader):
        filereader.seek(20, NOESEEK_REL) 
        
        count = filereader.readShort()   
        
        #start = timer() 
        
        while True:
            width = filereader.readShort()  
            height = filereader.readShort()
            
            imageData = self.getRGBData(filereader, width, height,  0)
            
            self.sprites.append(imageSprite(width, height, imageData))
            
            # sprite mask
            if self.spriteCount == 1 and count > 1:
                imageData = self.getRGBData(filereader, width, height, 1)
                self.sprites.append(imageSprite(width, height, imageData))
                
            if len(self.sprites) == count:
                break  
                
        #noesis.logPopup()                
        #end = timer()       
        #print(end - start) 
        
    def readDATASection(self, filereader):
        size = filereader.readUInt()
        filereader.seek(size, NOESEEK_REL)
        #filereader.seek(12, NOESEEK_REL)
        
        #count = filereader.readUInt() 
        
        #for i in range(count):
        #    filereader.seek(4, NOESEEK_REL) 
        
         #   dataSize = filereader.readUInt() 
        #    imageData = filereader.readBytes(dataSize)
        
         #   if self.spriteCount > 1:
         #       if self.paletteSize > 0:
         #           self.sprites.append(self.palettedToRGBA32(imageData))
            #else:
                #self.spriteData += palettedToRGBA32(imageData)
    
    def readPALSection(self, filereader):    
        filereader.seek(16, NOESEEK_REL)
        self.paletteSize = filereader.readUInt()
        self.palette = filereader.readBytes(self.paletteSize)       
        
    def getData(self):
        while not self.reader.checkEOF():
            id = self.reader.readUInt()
            if id == 1179800915: # SURF           
                self.readSURFSection(self.reader)
            elif id == 1096040772:  # DATA               
                self.readDATASection(self.reader)
            elif id == 541868368: # PAL
                self.readPALSection(self.reader)
            else:
                noesis.doException("Wrong format?")
 
 
def locCheckType(data):
    locSpriteArchive = LLSpriteArchive(NoeBitStream(data))
    if locSpriteArchive.parseHeader() == 0:
        return 0
        
    return 1   
    
    
def locLoadRGBA(data, texList):
    locSpriteArchive = LLSpriteArchive(NoeBitStream(data))
    if locSpriteArchive.parseHeader() == 0:
        return 0
   
    locSpriteArchive.getData()
   
    #if locSpriteArchive.spriteCount > 1:
    for sprite in locSpriteArchive.sprites:
        texList.append(NoeTexture("locolandtex", sprite.width, \
            sprite.height, sprite.data, \
            noesis.NOESISTEX_RGBA32))  
            
    #else: 
    #    texList.append(NoeTexture("locolandtex", locSpriteArchive.width, \
    #        locSpriteArchive.height, locSpriteArchive.spriteData, \
    #        noesis.NOESISTEX_RGBA32))
        
    return 1    