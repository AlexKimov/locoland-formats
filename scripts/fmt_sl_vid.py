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
        self.palette = []         
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
            formats = ["b4g4r4a4", "r5g6b5"] 
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
        
    def unpackSpriteData(self, filereader, width, height, dataSize): 
        spriteData = bytearray(width*height*4)
    
        if dataSize == 6:
            filereader.seek(2, NOESEEK_REL)
        else:           
            # some data
            dcount = filereader.readUShort()
            filereader.seek(dcount*6, NOESEEK_REL) 
            
            yoffset = filereader.readUShort() # lines to skip from the beginning              
            spriteLineNumber = filereader.readUShort() # number of the lines in sprite 
            
            for line in range(spriteLineNumber):
                xoffset = filereader.readUByte() # offset to where image data begins
                count = filereader.readUByte() # number of pixels to copy   
                
                # if xoffset = 0 - no more data in line or empty line  
                
                offsetInLine = 0 
 
                while xoffset >= 0:
                    if xoffset == 0 and count == 0:
                        break               
                
                    offsetInLine += xoffset                
                    pixelOffset = 0
                      
                    if self.type0 == 13 or self.type0 == 29:
                        filereader.seek(count*2, NOESEEK_REL)  
                        
                    for pixelIndex in range(count):                                       
                        # calculate position of image data to copy data
                        # line - current line
                        # yoffset - lines to skip from the beginning 
                        # offsetInLine - start position in line
                        # pixelIndex - current pixel pos to copy
                        index = filereader.readUByte()                        
                       
                        pos = 4*((line + yoffset)*self.width + offsetInLine + \
                            pixelIndex)
                        
                        spriteData[pos: pos + 4] = self.palette[index]
                    
                    offsetInLine += count 
                  
                    xoffset = filereader.readUByte()# if xoffset = 0 - end of line
                    count = filereader.readUByte()
                            
        self.sprites.append(imageSprite(width, height, spriteData))
        
    def readDATASection(self, filereader):
        filereader.seek(12, NOESEEK_REL)
        
        count = filereader.readUInt() 
        for i in range(count):
            dataSize = filereader.readUInt()        
            
            if self.type0 == 33:
                width = filereader.readUShort()
                height = filereader.readUShort()
                                
                self.unpackSpriteData(filereader, width, height, dataSize)
            else:
                filereader.seek(4, NOESEEK_REL)
                self.unpackSpriteData(filereader, self.width, self.height, \
                    dataSize)
                         
    def readPALSection(self, filereader):    
        filereader.seek(16, NOESEEK_REL)
        self.paletteSize = filereader.readUInt()

        alpha = (255).to_bytes(1, byteorder = "little") 
        
        for i in range(256):        
            if (self.paletteSize == 768):                                
                self.palette.append(filereader.readBytes(3) + alpha)
            else:
                self.palette.append(filereader.readBytes(4))            
        
        #noesis.logPopup()
        #print(len(self.palette))
        
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
                #noesis.doException("Wrong format?")
                break
 
 
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